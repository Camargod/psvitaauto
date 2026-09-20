# Research: aa-service-discovery

Date: 2026-09-16
Question: exact ServiceDiscoveryResponse a working head unit sends (why a real phone closes with "PROTOCOL_WRONG_CONFIGURATION" right after our SDR)

## Findings

### TL;DR / answer up front

A working head unit advertises **at least 6 channels**, not 2. Our HU advertises only
`video` (ch 3) + `input` (ch 1) and the phone closes immediately with
"PROTOCOL_WRONG_CONFIGURATION". The phone's media/projection endpoint set requires
**audio sinks** and a **sensor** channel to be present. The single most important
missing item is the **SYSTEM audio channel (16 kHz / 16-bit / mono)**: open-headunit's
source comments state explicitly that the SYSTEM sink is required "to keep the
connection alive" and that without a valid 16 kHz mono SYSTEM config the phone "empties
its endpoint list and the session ends with 'No audio/mic'". A video-only head unit
must still advertise audio channels (and then simply discard the PCM it receives).

### (a) Channel list + channel_id values a working HU advertises

Channel IDs are **head-unit-defined** (the phone learns them from the
`ChannelDescriptor.channel_id` field and echoes them in `CHANNEL_OPEN_REQUEST` and
subsequent data-frame headers), so different implementations use different numbers and
all work. The canonical aasdk numbering (openauto, LIVI, and what our `video=3` already
matches) is:

| channel_id | service | descriptor field | purpose |
|---|---|---|---|
| 0 | CONTROL | (implicit, not in SDR) | control plane |
| 1 | SENSOR | `sensor_channel` | driving status / night mode |
| 3 | VIDEO | `av_channel` (media sink) | H.264/H.265 main display |
| 4 | MEDIA_AUDIO | `av_channel` | music/podcast PCM |
| 5 | SPEECH_AUDIO | `av_channel` | navigation prompts |
| 6 | SYSTEM_AUDIO | `av_channel` | system sounds (required) |
| 8 | INPUT | `input_channel` | touch + keycodes |
| 9 | MIC_INPUT | `av_input_channel` (media source) | microphone (optional) |
| 10 | BLUETOOTH | `bluetooth_channel` | pairing (optional) |
| 12 | NAVIGATION | `navigation_channel` | turn-by-turn status (optional) |
| 13 | MEDIA_INFO | `media_info_channel` | playback status (optional) |
| 14 | PHONE_STATUS | `phone_status_channel` | call status (optional) |

Evidence for IDs:
- openauto `VideoService/InputService/AudioService/SensorService/BluetoothService.cpp`
  use `channel_->getId()` from aasdk `ChannelId` enum (VIDEO=3, MEDIA_AUDIO=4,
  SPEECH_AUDIO=5, SYSTEM_AUDIO=6, INPUT=8, SENSOR=1, BLUETOOTH=10).
- LIVI `stack/constants.ts` `CH` object: `SENSOR:1, VIDEO:3, MEDIA_AUDIO:4,
  SPEECH_AUDIO:5, SYSTEM_AUDIO:6, INPUT:8, MIC_INPUT:9, BLUETOOTH:10, NAVIGATION:12,
  MEDIA_INFO:13, PHONE_STATUS:14`.
- open-headunit `Channel.kt` uses a *different* compact numbering (SEN=1, VID=2, INP=3,
  AU1=4, AU2=5, AUD=6, MIC=7, BTH=8, MPB=9, NAV=10) — proof IDs are arbitrary.
- uglyoldbob/android-auto `lib.rs` assigns IDs by handler array index
  (Input=1, Sensor=2, Video=3, MediaAudio=4, SpeechAudio=5, SystemAudio=6, Mic=7, BT=8,
  Nav=9, MediaStatus=10) — further proof IDs are arbitrary, but note it still keeps
  VIDEO=3, MEDIA=4, SPEECH=5, SYSTEM=6.

**Our bug:** our input channel used `channel_id=1`, which collides with the SENSOR slot
and is inconsistent with the canonical INPUT=8. (Not fatal by itself, but wrong.) The
fatal problem is the absence of audio + sensor channels (see (f)).

### (b) Video channel: AVChannel / MediaSinkService fields + VideoConfig values

Wire message (field numbers from open-headunit `control.proto` `Service.MediaSinkService`
and LIVI `oaa/av/AVChannelData.proto` — identical field numbers):

```
AVChannel / MediaSinkService:
  1 available_type  (MediaCodecType)  — open-headunit/LIVI set this to the CODEC (3=H264_BP, 7=H265)
       (older aasdk/openauto + the oaa reverse-engineering name field 1 "stream_type" with VIDEO=3;
        both put 3 on the wire for H.264 video, 1 for PCM audio — same bytes)
  2 audio_type      (AudioStreamType) — NONE(0) for video
  3 audio_configs   (repeated)        — empty for video
  4 video_configs   (repeated)        — one per codec/resolution
  5 available_while_in_call (bool)    — true
```

`VideoConfig` (field numbers: open-headunit `VideoConfiguration`, LIVI `oaa/video/VideoConfigData.proto`,
uglyoldbob `Wifi.proto`):

```
  1 video_resolution / codec_resolution  = 2   (VIDEO_1280x720 / _1280x720)
  2 video_fps / frame_rate               = 2   (_30; NOTE _60=1, _30=2 in ALL modern sources)
  3 margin_width                         = 0   (or letterbox width)
  4 margin_height                        = 0
  5 dpi / density                        = 240 (valid Android density bucket; open-headunit uses real densityDpi)
  6 decoder_additional_depth / additional_depth = 0 (optional)
  8 pixel_aspect_ratio_e4                = 10000 (square)  [modern field; open-headunit/LIVI]
 10 video_codec_type / codec             = 3 (MEDIA_CODEC_VIDEO_H264_BP)  [modern field]
```

Concrete values used by working HUs:
- openauto `VideoService.cpp::fillFeatures`: `stream_type=VIDEO(3)`,
  `available_while_in_call=true`, video_configs = `{video_resolution, video_fps,
  margin_width, margin_height, dpi}` (no codec field — negotiated later in AV setup).
- open-headunit `ServiceDiscoveryResponse.kt`: `availableType=MEDIA_CODEC_VIDEO_H264_BP(3)`,
  `audioType=NONE(0)`, `availableWhileInCall=true`, video configs =
  `{codecResolution=negotiated, frameRate(_30=2/_60=1), marginWidth, marginHeight,
  density=real dpi, pixelAspectRatioE4=10000, videoCodecType=H264_BP(3)}`.
- uglyoldbob `video.rs`: `stream_type=VIDEO(3)`, `audio_type=SYSTEM(2)` (quirky),
  `available_while_in_call=true`, video configs = `{resolution, fps, dpi, margin 0,0}`.

**Critical enum facts (modern AA 16.2/17.x):**
- `VideoResolution`: 800x480=1, 1280x720=2, 1920x1080=3 (LIVI `VideoResolutionEnum.proto`,
  open-headunit `VideoCodecResolutionType`). 720p = **2**.
- `VideoFPS`: `_60=1`, `_30=2` (LIVI oaa + TS, open-headunit). **30fps = 2**, NOT 1.
  (uglyoldbob's `Wifi.proto` has `_30=1` — an old/inverted value from AA protocol ~1.1;
  do NOT copy it.)
- `MediaCodecType`: PCM=1, AAC_LC=2, H264_BP=3, VP9=5, AV1=6, H265=7 (all sources agree).

### (c) Input channel values

Wire message (open-headunit `InputSourceService`, LIVI `oaa/input/InputChannelConfigData.proto`,
uglyoldbob `InputChannel`):

```
InputChannel:
  1 supported_keycodes   (repeated/packed)   Android keycodes the HU can send
  2 touch_screen_config  (TouchConfig)       width/height = VIDEO resolution (not physical panel)
  3 touch_pad_config     (TouchConfig)       optional, only for rotary/touchpad
```

- **Touch config must equal the negotiated VIDEO resolution** (1280x720), not the
  physical panel. open-headunit sets `touchscreen.width/height = negotiatedWidth/Height`
  (the video res). LIVI sets `touchW = vW - insetLeft - insetRight`, `touchH = vH -
  insetTop - insetBottom` (video res minus margins). **Our 960x544 touch config is
  inconsistent with our 720p video.**
- `supported_keycodes`: count is flexible. The Rust example advertises just
  `[1,2,3,4,5]` and works. open-headunit advertises ~50 codes (see
  `input/KeyCode.kt`: SOFT_LEFT/RIGHT, BACK, DPAD_*, MEDIA_*, SEARCH, CALL, VOLUME,
  ENTER, HOME, HEADSETHOOK, 0-9, STAR/POUND, custom 1/2/81/224, steering-wheel 264-271,
  rotary 65536-65538). LIVI advertises 48 codes including 3..26, 66, 79, 82, 84-91, 111,
  126, 127, 164, 219, 231, 260-263, 65536. The phone sends a `BINDING_REQUEST` for the
  subset it wants; the HU must have advertised exactly those, or binding fails.
- A minimal safe set for the Vita: DPAD_CENTER/UP/DOWN/LEFT/RIGHT (19-23), BACK(4),
  ENTER(66), HOME(3), MEDIA_PLAY_PAUSE/NEXT/PREVIOUS, plus 0-9, STAR, POUND. Touch is
  the primary input; keycodes can be minimal.

### (d) Audio descriptors + "must a video-only HU advertise audio?"

`AudioConfig` (all sources): `sample_rate=1, bit_depth=2, channel_count=3`.
`AudioStreamType` (open-headunit `media.proto`, LIVI oaa `AudioTypeEnum.proto`):
NONE=0, SPEECH=1, SYSTEM=2, MEDIA=3, ALARM=4. (open-headunit additionally defines
GUIDANCE=5/ANNOUNCEMENT=6/RING=7 but uses SPEECH=1 for the nav-audio channel; LIVI TS
names the value 1 "GUIDANCE" — same wire value.)

Working audio descriptors (identical across openauto/LIVI/open-headunit/uglyoldbob):

| channel (id) | audio_type | sample_rate | bit_depth | channels |
|---|---|---|---|---|
| MEDIA_AUDIO (4) | MEDIA (3) | 48000 | 16 | 2 |
| SPEECH_AUDIO (5) | SPEECH (1) | 16000 | 16 | 1 |
| SYSTEM_AUDIO (6) | SYSTEM (2) | 16000 | 16 | 1 |
| MIC_INPUT (9, optional) | (media source) | 16000 | 16 | 1 |

Each audio `av_channel` also sets `available_type = MEDIA_CODEC_AUDIO_PCM(1)`,
`available_while_in_call = true`, and (open-headunit) `audioType` as above.

**Yes — a video-only head unit must still advertise audio channels.** Direct evidence
from open-headunit `ServiceDiscoveryResponse.kt` + `AudioConfigs.kt`:
- "`// Always add Audio2 (System Sounds) to keep connection alive`"
- "`// 16 kHz mono is required here, not preferred. The phone accepts the SYSTEM sink
  only if it offers that config ... so anything else empties its endpoint list and the
  session ends with "No audio/mic"`"

open-headunit, even with its audio sink disabled, always adds the SYSTEM audio channel.
Media + speech audio are also normally added (skipped only in self-projection mode).
The phone's projection endpoint list is built from the advertised media sinks; zero
audio sinks ⇒ the phone aborts session setup.

### (e) Other required ServiceDiscoveryResponse fields

`ServiceDiscoveryResponse` field numbers (open-headunit `control.proto`, LIVI
`aap_protobuf/service/control/message/ServiceDiscoveryResponse.proto` and
`oaa/control/ServiceDiscoveryResponseMessage.proto`):

```
  1 channels                          (repeated Service/ChannelDescriptor)
  2 head_unit_name / make
  3 car_model / model
  4 car_year / year
  5 car_serial / vehicle_id
  6 driver_position (enum)            LEFT=0, RIGHT=1, CENTER=2, UNKNOWN=3
  7 headunit_manufacturer / head_unit_make
  8 headunit_model
  9 sw_build / head_unit_software_build
 10 sw_version / head_unit_software_version
 11 can_play_native_media_during_vr (bool)
 12 hide_projected_clock (bool)        [open-headunit; dropped in LIVI's newest proto]
 13 session_configuration (int32)      bit flags: HIDE_CLOCK=1, HIDE_PHONE_SIGNAL=2,
                                      HIDE_BATTERY_LEVEL=4, CAN_PLAY_NATIVE_MEDIA_DURING_VR=8
 14 display_name (string)
 15 probe_for_support (bool)           false
 16 connection_configuration (ping)    [optional; LIVI sends timeout=5000/interval=1500/...]
 17 headunit_info (HeadUnitInfo)       {make,model,year,vehicle_id,head_unit_make,
                                      head_unit_model,head_unit_software_build,
                                      head_unit_software_version, vehicle_type}
```

**Schema-evolution warning:** field 6 is a `DriverPosition` **enum** in modern AA
(16.2/17.x), NOT the old aasdk `left_hand_drive_vehicle` bool. uglyoldbob's `Wifi.proto`
(AA ~1.1) still declares `left_hand_drive_vehicle` bool at field 6 and `hide_clock` at
12 — the OLD schema. Do not use the old schema against AA 17.x. There is **no**
phone-enforced minimum on `head_unit_name`/`car_model` etc. — the phone keys its stored
per-head-unit record on make/model/year/vehicle_id, so give them stable values
(open-headunit `VehicleIdentityPolicy`).

### (f) Why the phone closes on "wrong configuration"

"PROTOCOL_WRONG_CONFIGURATION" is a phone-side AA error emitted right after the SDR is
parsed and the projection configuration is built. The evidence points to **missing
channels**, in order of likelihood:

1. **No audio sinks** (primary cause). We advertise zero `media_sink_service` audio
   channels. The phone requires at least the SYSTEM audio sink (16 kHz mono) and, in
   practice, MEDIA + SPEECH too. Without any audio endpoint the phone tears the session
   down ("No audio/mic" per open-headunit's inline comments). This is the classic cause
   of the close-right-after-SDR symptom.
2. **No SENSOR channel (DRIVING_STATUS)**. Every working HU advertises a sensor channel
   with `DRIVING_STATUS` (and usually `NIGHT_DATA`). The phone needs driving status to
   determine the parking-brake safety gate before projecting. openauto
   (`SensorService.cpp`) advertises DRIVING_STATUS + NIGHT_DATA; open-headunit and
   uglyoldbob the same. We advertise none.
3. **Input touch config inconsistent with video res** (960x544 vs 720p) — a secondary
   inconsistency that can also trip config validation.
4. **Wrong/missing video fields** — we should confirm we set `video_fps = 2` (30fps, not
   1) and, on the modern schema, `video_codec_type = 3` (H264_BP) + `pixel_aspect_ratio_e4`.
   Wrong dpi is NOT the cause: 240 is a valid Android density bucket.

Fact vs hypothesis: items 1 and 2 are directly supported by open-headunit source
comments and by the fact that all four working implementations advertise audio + sensor.
Item 3/4 are inferred from cross-implementation consistency and flagged as such below.

## Sources (URLs)

- f-io/LIVI (Rust/TS stack) — https://github.com/f-io/LIVI
  - `src/main/services/projection/driver/aa/stack/session/ServiceDiscoveryBuilder.ts`
  - `src/main/services/projection/driver/aa/stack/constants.ts`
  - `src/main/services/projection/driver/aa/protos/oaa/control/ServiceDiscoveryResponseMessage.proto`
  - `.../oaa/control/ChannelDescriptorData.proto`
  - `.../oaa/av/AVChannelData.proto`, `.../oaa/video/VideoConfigData.proto`,
    `.../oaa/video/VideoResolutionEnum.proto`, `.../oaa/video/VideoFPSEnum.proto`,
    `.../oaa/av/MediaCodecTypeEnum.proto`, `.../oaa/audio/AudioTypeEnum.proto`,
    `.../oaa/audio/AudioConfigData.proto`, `.../oaa/input/InputChannelConfigData.proto`,
    `.../oaa/input/TouchScreenConfigData.proto`
  - `.../aap_protobuf/service/media/sink/MediaSinkService.proto`
  - `.../aap_protobuf/service/control/message/ServiceDiscoveryResponse.proto`
  - `.../aap_protobuf/channel/control/SessionConfiguration.proto`
- andreknieriem/open-headunit (Kotlin) — https://github.com/andreknieriem/open-headunit
  - `app/src/main/java/com/andrerinas/openheadunit/aap/protocol/messages/ServiceDiscoveryResponse.kt`
  - `app/src/main/java/com/andrerinas/openheadunit/aap/protocol/Channel.kt`
  - `app/src/main/java/com/andrerinas/openheadunit/aap/protocol/AudioConfigs.kt`
  - `app/src/main/java/com/andrerinas/openheadunit/input/KeyCode.kt`
  - `app/src/main/proto/control.proto`, `media.proto`, `common.proto`
- f1xpl/openauto (C++) — https://github.com/f1xpl/openauto
  - `src/autoapp/Service/AndroidAutoEntity.cpp`
  - `src/autoapp/Service/VideoService.cpp`, `InputService.cpp`, `AudioService.cpp`,
    `SensorService.cpp`, `BluetoothService.cpp`
- uglyoldbob/android-auto (Rust) — https://github.com/uglyoldbob/android-auto
  - `protobuf/Wifi.proto`
  - `src/lib.rs`, `src/video.rs`, `src/input.rs`, `src/mediaaudio.rs`, `src/sysaudio.rs`,
    `src/speechaudio.rs`, `src/sensor.rs`
  - `examples/main/main.rs`

## Implications for PSVitaAuto

1. We must advertise a **full minimum channel set**, not two channels:
   `SENSOR(1)`, `VIDEO(3)`, `MEDIA_AUDIO(4)`, `SPEECH_AUDIO(5)`, `SYSTEM_AUDIO(6)`,
   `INPUT(8)`. Audio is mandatory even though the Vita renders video only and audio
   stays on the car's Bluetooth — the AA audio channels are negotiated and the PCM
   discarded (openheadunit/LIVI both support "audio sink off" but STILL keep SYSTEM
   audio alive). We should follow the same pattern: advertise media+speech+system audio
   and drop the PCM we receive (or don't subscribe the phone-side media playback).
2. Fix `input channel_id` from 1 → 8.
3. Fix touch config to match the video resolution (720p ⇒ 1280x720), not 960x544.
4. Confirm video fields on the modern schema: `video_resolution=2`, `video_fps=2`
   (30fps — NOT 1), `dpi=240`, margins 0, `video_codec_type=3` (H264_BP),
   `available_while_in_call=true`, `audio_type=NONE(0)` on the video channel.
5. Add a SENSOR channel with `DRIVING_STATUS` (and `NIGHT_DATA`) and answer
   `SensorStartRequest` with DRIVING_STATUS=UNRESTRICTED(0) so the parking gate passes.
6. Use the modern `ServiceDiscoveryResponse` schema (field 6 = `driver_position` enum,
   field 13 `session_configuration`, field 17 `headunit_info`), not the old aasdk
   `left_hand_drive_vehicle` bool.

## Remaining unknowns

- The exact meaning of the literal string "PROTOCOL_WRONG_CONFIGURATION" (it is a
  phone/AA-side error string not present in any public HU source; our attribution to
  "missing audio/sensor" is inference from open-headunit's "No audio/mic" comments, not
  a confirmed mapping to that exact string).
- Whether the phone additionally rejects a SDR that omits the optional channels
  (MIC_INPUT, BLUETOOTH, NAVIGATION, MEDIA_INFO) on newer AA builds — open-headunit adds
  media-playback + navigation status unconditionally and mic/BT conditionally, so the
  safest is to mirror open-headunit's full set.
- Whether 960x544 can be advertised as a portrait/native resolution (not a standard AA
  tier) or whether we must stick to a standard tier (800x480 or 1280x720) + letterbox
  margins. AA only supports the enumerated VideoResolution tiers; 960x544 is not one.
- The precise minimum `supported_keycodes` the phone tolerates before declaring the
  input channel invalid.

## Recommended next steps

1. **Spike (prototype):** re-send the SDR with the full 6-channel minimum set
   (sensor/video/media/speech/system/input), modern schema field numbers, and the exact
   values in (a)-(e), then re-test against the AA 17.x phone. If the phone proceeds to
   `CHANNEL_OPEN_REQUEST`, the "wrong configuration" was the missing audio/sensor set.
2. If it still closes, bisect: (i) add only SYSTEM audio; (ii) add sensor channel;
   (iii) fix touch config; (iv) verify `video_fps=2` and `video_codec_type=3` on the
   wire (hex-dump the encoded SDR and compare field 4 sub-fields).
3. Mirror open-headunit's full channel set (add MEDIA_INFO ch 13 + NAVIGATION ch 12 +
   optional BLUETOOTH ch 10) once the minimum set connects, to maximize phone
   compatibility.
4. Decide the Vita video tier: recommend advertising `1280x720 @ 30 fps, dpi 240,
   margins 0` and scaling to the 960x544 panel in the renderer (touch coordinates
   remapped 1280x720 → 960x544), or `800x480` with the same remap — a separate spike
   for the video pipeline.

What the feature spec can rely on (source-backed): the channel list + IDs, the exact
VideoConfig/AVChannel/AudioConfig/InputChannel field numbers and values, the audio
types/configs, and the fact that audio channels are mandatory. What still needs a spike
(prototype): the exact reason-string mapping and the specific phone behavior for the
optional channels / 960x544 tier.
