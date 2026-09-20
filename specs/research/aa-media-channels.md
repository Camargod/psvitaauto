# Research: aa-media-channels

Date: 2026-09-16
Question: exact AA media channel lifecycle for a head unit

## Findings

All findings are facts read from `f1xpl/aasdk` (branch `development`) and
`f1xpl/openauto` (branch `master`), cited per-file. Hypotheses are labelled
explicitly. This note covers the AV/audio/video/input channel lifecycle
**after** the control handshake and per-channel open; the control handshake and
channel-open framing are already covered in `specs/research/aa-control-handshake.md`
and `specs/research/aa-framing.md`, and are cross-referenced rather than repeated.

### 0. Channel id map (recap from `ChannelId.hpp`)

```
CONTROL=0, INPUT=1, SENSOR=2, VIDEO=3, MEDIA_AUDIO=4, SPEECH_AUDIO=5,
SYSTEM_AUDIO=6, AV_INPUT=7, BLUETOOTH=8, NONE=255
```

`channel_id` is written on the wire as `static_cast<uint32_t>(channel_->getId())`
in each `fillFeatures` (`VideoService.cpp`, `AudioService.cpp`, `InputService.cpp`).

### 1. Channel open for media (post-discovery, per channel)

The open is **phone-driven and per-channel, with no enforced ordering in the HU**.
After the control channel is active and the HU has sent `SERVICE_DISCOVERY_RESPONSE`,
the phone sends a `CHANNEL_OPEN_REQUEST` (Control message id 0x0007) for each
channel it needs. Each `CHANNEL_OPEN_REQUEST` is carried **on the target channel's
frame-header channel id** (not CONTROL=0), with `MessageType::CONTROL` and
`EncryptionType::ENCRYPTED` (already documented in `aa-control-handshake.md` §7).

Each channel's service channel dispatches it to `onChannelOpenRequest`, and the HU
answers with `CHANNEL_OPEN_RESPONSE` (0x0008, `MessageType::CONTROL`, ENCRYPTED):

- `VideoService::onChannelOpenRequest` (`openauto/src/autoapp/Service/VideoService.cpp`):
  `status = videoOutput_->open() ? Status::OK : Status::FAIL`.
- `AudioService::onChannelOpenRequest` (`AudioService.cpp`): same pattern via
  `audioOutput_->open()`.
- `InputService::onChannelOpenRequest` (`InputService.cpp`): always `Status::OK`.

The channels involved in media are: **VIDEO=3, MEDIA_AUDIO=4, SPEECH_AUDIO=5,
SYSTEM_AUDIO=6, INPUT=1** (plus AV_INPUT=7 for mic, out of scope here). The exact
open order is decided by the phone; openauto registers all services at startup
(`AndroidAutoEntity::start` → each `IService::start` → `channel_->receive(...)`) so
it is ready to answer opens on any channel in any order. There is no
source-enforced sequence; see "Remaining unknowns".

After a channel opens, the phone moves to AV setup (video + audio channels), or to
binding (input channel). Details per channel follow.

### 2. AVChannelSetupRequest / AVChannelSetupResponse

Message ids (`aasdk_proto/AVChannelMessageIdsEnum.proto`):

```
AV_MEDIA_WITH_TIMESTAMP_INDICATION = 0x0000
AV_MEDIA_INDICATION                = 0x0001
SETUP_REQUEST                      = 0x8000
START_INDICATION                   = 0x8001
STOP_INDICATION                    = 0x8002
SETUP_RESPONSE                     = 0x8003
AV_MEDIA_ACK_INDICATION            = 0x8004
AV_INPUT_OPEN_REQUEST              = 0x8005
AV_INPUT_OPEN_RESPONSE             = 0x8006
VIDEO_FOCUS_REQUEST                = 0x8007
VIDEO_FOCUS_INDICATION             = 0x8008
```

Messages (`AVChannelSetupRequestMessage.proto`, `AVChannelSetupResponseMessage.proto`):

```proto
// proto3 — phone → HU
message AVChannelSetupRequest { uint32 config_index = 1; }

// proto2 — HU → phone
message AVChannelSetupResponse {
    required enums.AVChannelSetupStatus.Enum media_status = 1;
    required uint32 max_unacked = 2;
    repeated uint32 configs = 3;
}
```

`AVChannelSetupStatus` (`AVChannelSetupStatusEnum.proto`): `NONE = 0`, `FAIL = 1`, `OK = 2`.

**Phone side**: after `CHANNEL_OPEN_RESPONSE`, the phone sends `SETUP_REQUEST`
(0x8000) carrying the `config_index` it selected (an index into the `video_configs`
/ `audio_configs` list the HU advertised in discovery). The HU parses
`request.config_index()` but in openauto ignores its value.

**HU side** (`VideoService::onAVChannelSetupRequest`, `AudioService::onAVChannelSetupRequest`):

```cpp
AVChannelSetupResponse response;
response.set_media_status(OK);   // video: init()?OK:FAIL ; audio: always OK
response.set_max_unacked(1);
response.add_configs(0);         // openauto adds exactly one entry: 0
channel_->sendAVChannelSetupResponse(response, ...);
```

So the canonical HU reply is `media_status = OK(2)`, `max_unacked = 1`,
`configs = [0]`. `SETUP_RESPONSE` is sent with `MessageType::SPECIFIC` and
`EncryptionType::ENCRYPTED` (`VideoServiceChannel.cpp` / `AudioServiceChannel.cpp`
`sendAVChannelSetupResponse`).

**Which channels use AV setup vs. just start**: every **AV channel** — video (3),
media audio (4), speech audio (5), system audio (6), and AV_INPUT mic (7) — uses the
`SETUP_REQUEST`/`SETUP_RESPONSE` handshake (all dispatch `SETUP_REQUEST` in their
`messageHandler`). The **input channel (INPUT=1) does not** use AV setup; it uses
open → `BINDING_REQUEST`/`BINDING_RESPONSE` instead (see §7). After setup, the phone
sends `START_INDICATION` (there is no separate "start" for the input channel).

### 3. Video focus

Messages (`VideoFocusRequestMessage.proto`, `VideoFocusIndicationMessage.proto`,
`VideoFocusModeEnum.proto`, `VideoFocusReasonEnum.proto`):

```proto
// proto3 — phone → HU
message VideoFocusRequest {
    int32 disp_index = 1;
    enums.VideoFocusMode.Enum focus_mode = 2;
    enums.VideoFocusReason.Enum focus_reason = 3;
}
// proto3 — HU → phone
message VideoFocusIndication {
    enums.VideoFocusMode.Enum focus_mode = 1;
    bool unrequested = 2;
}

message VideoFocusMode   { enum Enum { NONE = 0; FOCUSED = 1; UNFOCUSED = 2; } }
message VideoFocusReason { enum Enum { NONE = 0; UNK_1 = 1; UNK_2 = 2; } }
```

- **Who requests**: the **phone** sends `VIDEO_FOCUS_REQUEST` (0x8007) to request a
  focus change (carries `disp_index`, `focus_mode`, `focus_reason`). The **HU
  grants/notifies** by sending `VIDEO_FOCUS_INDICATION` (0x8008).
- `VideoService::onVideoFocusRequest` (`VideoService.cpp`) just logs and calls
  `sendVideoFocusIndication()`, which sends `focus_mode = FOCUSED`, `unrequested = false`.
- **Additionally**, the HU *proactively* sends `VIDEO_FOCUS_INDICATION` immediately
  after the AV setup response completes: `onAVChannelSetupRequest` chains
  `sendAVChannelSetupResponse(...).then(sendVideoFocusIndication)` — i.e. after the
  setup response is flushed, the HU grants focus (`FOCUSED`, `unrequested=false`).
- openauto always sets `unrequested = false`. The `unrequested` field distinguishes
  a HU-initiated focus grant from a reply to a phone request; openauto never uses
  `true`.

Both focus messages are `MessageType::SPECIFIC`, `EncryptionType::ENCRYPTED`.

### 4. Start / stop

Messages (`AVChannelStartIndicationMessage.proto`, `AVChannelStopIndicationMessage.proto`):

```proto
// proto3 — phone → HU
message AVChannelStartIndication { int32 session = 1; uint32 config = 2; }
// proto3 — phone → HU
message AVChannelStopIndication { /* empty */ }
```

- `START_INDICATION` (0x8001) is sent by the **phone** when it begins streaming on a
  channel. It carries `session` (a phone-assigned `int32` session id the HU must
  echo back in every `AV_MEDIA_ACK_INDICATION`) and `config` (the selected config
  index). `VideoService::onAVChannelStartIndication` stores `session_`; the audio
  variant also calls `audioOutput_->start()`.
- `STOP_INDICATION` (0x8002) is sent by the phone to stop a stream. The audio
  services handle it: `session_ = -1; audioOutput_->suspend();`. Note: openauto's
  `VideoService` (master) declares `onAVChannelStartIndication` but **does not
  implement `onAVChannelStopIndication`** even though
  `IVideoServiceChannelEventHandler` requires it — i.e. video stop is effectively a
  no-op in this reference implementation (the interface lists it; the VideoService
  override list omits it). A correct HU should still handle video stop (reset
  `session`, stop rendering).

Both are `MessageType::SPECIFIC`, `EncryptionType::ENCRYPTED`.

### 5. Video frames (H.264) — arrival and encryption flag

`VideoServiceChannel::messageHandler` (`VideoServiceChannel.cpp`) handles two media
message ids as **raw bytes, not protobuf**:

- `AV_MEDIA_WITH_TIMESTAMP_INDICATION` (0x0000): payload = **8-byte big-endian
  timestamp** (`Timestamp::ValueType = uint64_t`; `Timestamp.cpp` uses
  `boost::endian::native_to_big`) followed by the raw H.264 data. The channel strips
  the first `sizeof(Timestamp::ValueType)` bytes as the timestamp and passes the rest
  as the frame buffer (`handleAVMediaWithTimestampIndication`).
- `AV_MEDIA_INDICATION` (0x0001): payload = **raw H.264 data with no timestamp**.
  openauto treats it as `timestamp = 0` (`VideoService::onAVMediaIndication` →
  `onAVMediaWithTimestampIndication(0, buffer)`; `AudioService::onAVMediaIndication`
  does the same).

openauto hands the raw buffer straight to `videoOutput_->write(timestamp, buffer)`
(hardware H.264 decode). There is **no protobuf envelope and no per-frame media
header** other than the optional 8-byte timestamp.

**Encryption flag**: the `ENCRYPTED` bit (0x08) in the frame-header flags byte is set
by the **sender** (the phone) on the wire; the HU-side `MessageInStream` decrypts a
frame only when that bit is set, otherwise it passes the raw payload through
(`MessageInStream.cpp`, `receiveFramePayloadHandler`). aasdk/openauto is the HU side
and does not itself set this bit on receive. **The established behavior — and the
only configuration under which openauto works, since it feeds the buffer to the
decoder with no extra decryption — is that the phone sends AV media (video H.264 and
audio PCM) frames with the `ENCRYPTED` bit CLEAR (i.e. PLAIN, raw bytes), while
protobuf control/specific messages are ENCRYPTED.** This is a widely-reported AA
protocol fact, but it is not directly observable from the HU-side code alone and is
labelled **hypothesis-to-confirm-by-capture** (see "Remaining unknowns").

**Does the HU ACK video frames?** Yes — for **every** media frame received (both
0x0000 and 0x0001), the HU sends `AV_MEDIA_ACK_INDICATION` (0x8004) with
`session = session_`, `value = 1` (`VideoService::onAVMediaWithTimestampIndication`
and `onAVMediaIndication`). `max_unacked = 1` in setup means at most 1 unacknowledged
frame is allowed, so the ACK must be sent promptly after each frame.

### 6. Audio channels, ACK cadence, and non-rendering behavior

- **Channels**: MEDIA_AUDIO (4) = media audio, SPEECH_AUDIO (5) = speech audio,
  SYSTEM_AUDIO (6) = system audio (`MediaAudioServiceChannel.cpp`,
  `SpeechAudioServiceChannel.cpp`, `SystemAudioServiceChannel.cpp` are trivial
  subclasses of `AudioServiceChannel` differing only in `ChannelId`). `AudioType`
  (`AudioTypeEnum.proto`): `NONE=0, SPEECH=1, SYSTEM=2, MEDIA=3, ALARM=4`; the
  `fillFeatures` maps system→SYSTEM, media→MEDIA, speech→SPEECH.
- **Audio frame arrival**: identical to video — `AV_MEDIA_WITH_TIMESTAMP_INDICATION`
  (0x0000) or `AV_MEDIA_INDICATION` (0x0001), raw audio bytes (PCM per the advertised
  `AudioConfig`), same `ENCRYPTED`-bit-cleared expectation as §5.
- **ACK cadence**: exactly like video. After **each** audio frame, the HU sends
  `AV_MEDIA_ACK_INDICATION` (`session = session_`, `value = 1`). `max_unacked = 1`
  in the audio setup response means the phone stops sending after 1 unacked frame,
  so ACKing every frame is required to keep the audio (and the session) flowing.
- **What the HU must do even if it does not render audio** (PSVitaAuto charter:
  audio stays on car Bluetooth; the Vita negotiates audio but does not output it):
  1. Advertise the audio channel descriptors in discovery (else the phone may not
     open them and may refuse to proceed), OR omit them if the phone permits.
  2. Answer `CHANNEL_OPEN_RESPONSE` (OK) on each audio channel.
  3. Answer `SETUP_REQUEST` with `SETUP_RESPONSE` (`media_status=OK, max_unacked=1,
     configs=[0]`).
  4. Record `session` from `START_INDICATION`.
  5. **Consume and ACK every audio media frame** with `AV_MEDIA_ACK_INDICATION`
     (`session`, `value=1`) — discarding the payload instead of playing it. Skipping
     this stalls flow control (max_unacked=1) and can freeze the phone's AA session.
  6. Handle `STOP_INDICATION` (reset `session`).
  7. (Control channel, not this note's scope) answer `AUDIO_FOCUS_REQUEST` with
     `AUDIO_FOCUS_RESPONSE` as in `aa-control-handshake.md`.

openauto's `AudioService::onAVMediaWithTimestampIndication` performs the ACK
regardless of whether the underlying `IAudioOutput` actually plays — a no-op/discard
`IAudioOutput` still produces the ACK, which is exactly the pattern PSVitaAuto needs.

### 7. Input channel flow

Message ids (`aasdk_proto/InputChannelMessageIdsEnum.proto`):

```
NONE = 0x0000; INPUT_EVENT_INDICATION = 0x8001; BINDING_REQUEST = 0x8002; BINDING_RESPONSE = 0x8003;
```

- After the input channel (INPUT=1) opens (`CHANNEL_OPEN_RESPONSE` OK), the phone
  sends `BINDING_REQUEST` (0x8002) carrying `repeated int32 scan_codes`
  (`BindingRequestMessage.proto`) — the phone asks the HU to bind a set of button
  scan codes.
- The HU answers `BINDING_RESPONSE` (0x8003) with `status` (`BindingResponseMessage.proto`,
  `Status::OK/FAIL`). openauto's `InputService::onBindingRequest`
  (`InputService.cpp`) validates every requested scan code against
  `inputDevice_->getSupportedButtonCodes()`; if all match → `Status::OK` and
  `inputDevice_->start(*this)` (begins generating input events); otherwise `FAIL`.
  All input messages are `MessageType::SPECIFIC`, `EncryptionType::ENCRYPTED`.
- After binding, the HU streams input back to the phone as `INPUT_EVENT_INDICATION`
  (0x8001, `InputEventIndicationMessage.proto`):

```proto
message InputEventIndication {
    uint64 timestamp = 1;
    int32 disp_channel = 2;
    data.TouchEvent touch_event = 3;
    data.ButtonEvents button_event = 4;
    data.AbsoluteInputEvents absolute_input_event = 5;
    data.RelativeInputEvents relative_input_event = 6;
}
```

openauto's `InputService::onTouchEvent` builds a `touch_event` with `touch_action`
(`TouchActionEnum`: PRESS=0, RELEASE=1, DRAG=2), one `touch_location`
(`TouchLocationData`: `x`, `y`, `pointer_id=0`). `onButtonEvent` builds a
`button_event` with `scan_code`, `is_pressed`, `meta=0`, `long_press=false`, or a
`relative_input_event` for the SCROLL_WHEEL code. `timestamp` is µs since epoch.

## Sources (URLs)

aasdk (`f1xpl/aasdk`, branch `development`):
- https://raw.githubusercontent.com/f1xpl/aasdk/development/include/f1x/aasdk/Messenger/ChannelId.hpp
- https://raw.githubusercontent.com/f1xpl/aasdk/development/src/Channel/AV/AVInputServiceChannel.cpp
- https://raw.githubusercontent.com/f1xpl/aasdk/development/src/Channel/AV/VideoServiceChannel.cpp
- https://raw.githubusercontent.com/f1xpl/aasdk/development/src/Channel/AV/AudioServiceChannel.cpp
- https://raw.githubusercontent.com/f1xpl/aasdk/development/src/Channel/AV/MediaAudioServiceChannel.cpp
- https://raw.githubusercontent.com/f1xpl/aasdk/development/src/Channel/AV/SpeechAudioServiceChannel.cpp
- https://raw.githubusercontent.com/f1xpl/aasdk/development/src/Channel/AV/SystemAudioServiceChannel.cpp
- https://raw.githubusercontent.com/f1xpl/aasdk/development/src/Channel/Input/InputServiceChannel.cpp
- https://raw.githubusercontent.com/f1xpl/aasdk/development/src/Channel/Control/ControlServiceChannel.cpp
- https://raw.githubusercontent.com/f1xpl/aasdk/development/src/Channel/ServiceChannel.cpp
- https://raw.githubusercontent.com/f1xpl/aasdk/development/include/f1x/aasdk/Channel/AV/IVideoServiceChannelEventHandler.hpp
- https://raw.githubusercontent.com/f1xpl/aasdk/development/include/f1x/aasdk/Channel/AV/IAudioServiceChannelEventHandler.hpp
- https://raw.githubusercontent.com/f1xpl/aasdk/development/include/f1x/aasdk/Channel/AV/IAVInputServiceChannelEventHandler.hpp
- https://raw.githubusercontent.com/f1xpl/aasdk/development/src/Messenger/Messenger.cpp
- https://raw.githubusercontent.com/f1xpl/aasdk/development/src/Messenger/MessageInStream.cpp
- https://raw.githubusercontent.com/f1xpl/aasdk/development/src/Messenger/MessageOutStream.cpp
- https://raw.githubusercontent.com/f1xpl/aasdk/development/src/Messenger/Message.cpp
- https://raw.githubusercontent.com/f1xpl/aasdk/development/src/Messenger/FrameHeader.cpp
- https://raw.githubusercontent.com/f1xpl/aasdk/development/src/Messenger/FrameSize.cpp
- https://raw.githubusercontent.com/f1xpl/aasdk/development/src/Messenger/Timestamp.cpp
- https://raw.githubusercontent.com/f1xpl/aasdk/development/src/Messenger/MessageId.cpp
- https://raw.githubusercontent.com/f1xpl/aasdk/development/include/f1x/aasdk/Messenger/EncryptionType.hpp
- https://raw.githubusercontent.com/f1xpl/aasdk/development/include/f1x/aasdk/Messenger/MessageType.hpp
- https://raw.githubusercontent.com/f1xpl/aasdk/development/include/f1x/aasdk/Messenger/FrameType.hpp

openauto (`f1xpl/openauto`, branch `master`):
- https://raw.githubusercontent.com/f1xpl/openauto/master/src/autoapp/Service/AndroidAutoEntity.cpp
- https://raw.githubusercontent.com/f1xpl/openauto/master/src/autoapp/Service/VideoService.cpp
- https://raw.githubusercontent.com/f1xpl/openauto/master/include/f1x/openauto/autoapp/Service/VideoService.hpp
- https://raw.githubusercontent.com/f1xpl/openauto/master/src/autoapp/Service/AudioService.cpp
- https://raw.githubusercontent.com/f1xpl/openauto/master/src/autoapp/Service/MediaAudioService.cpp
- https://raw.githubusercontent.com/f1xpl/openauto/master/src/autoapp/Service/SpeechAudioService.cpp
- https://raw.githubusercontent.com/f1xpl/openauto/master/src/autoapp/Service/SystemAudioService.cpp
- https://raw.githubusercontent.com/f1xpl/openauto/master/src/autoapp/Service/InputService.cpp
- https://raw.githubusercontent.com/f1xpl/openauto/master/src/autoapp/Projection/VideoOutput.cpp

Local vendored protos (`third_party/aasdk_proto/`): AVChannelMessageIdsEnum.proto,
AVChannelSetupRequestMessage.proto, AVChannelSetupResponseMessage.proto,
AVChannelSetupStatusEnum.proto, AVChannelStartIndicationMessage.proto,
AVChannelStopIndicationMessage.proto, AVMediaAckIndicationMessage.proto,
VideoFocusRequestMessage.proto, VideoFocusIndicationMessage.proto,
VideoFocusModeEnum.proto, VideoFocusReasonEnum.proto, AVChannelData.proto,
VideoConfigData.proto, AudioConfigData.proto, AVStreamTypeEnum.proto,
AudioTypeEnum.proto, AVInputChannelData.proto, AVInputOpenRequestMessage.proto,
AVInputOpenResponseMessage.proto, InputChannelData.proto,
InputChannelMessageIdsEnum.proto, InputEventIndicationMessage.proto,
BindingRequestMessage.proto, BindingResponseMessage.proto, StatusEnum.proto,
TouchEventData.proto, TouchLocationData.proto, TouchActionEnum.proto,
ButtonEventsData.proto, ButtonEventData.proto, AbsoluteInputEventsData.proto,
RelativeInputEventsData.proto, ChannelOpenRequestMessage.proto,
ChannelOpenResponseMessage.proto, ChannelDescriptorData.proto,
ControlMessageIdsEnum.proto.

Local cross-references: `specs/research/aa-control-handshake.md`,
`specs/research/aa-framing.md`, `specs/research/aa-tls-mechanics.md`.

## Implications for PSVitaAuto

1. **Sequence is a small, deterministic state machine** per channel. Video/audio:
   `OPEN_RESPONSE → SETUP_RESPONSE → (video: FOCUS_INDICATION) → START → media frames
   (+ ACK each) → STOP`. Input: `OPEN_RESPONSE → BINDING_RESPONSE → INPUT_EVENT_INDICATION…`.
   This maps cleanly to C state machines on the Vita.
2. **Flow control is the hard requirement**: `max_unacked = 1` and `ACK value = 1`
   per frame means the Vita **must ACK every video and audio frame immediately** or
   the phone stalls. The ACK payload is trivial (`AVMediaAckIndication{session, value=1}`),
   ENCRYPTED, SPECIFIC.
3. **H.264 frames are raw**: after the 2-byte frame header + size + 2-byte message id
   prefix, the frame is `[8-byte BE timestamp]? + Annex-B H.264 NAL stream`. No
   protobuf. The Vita's decoder should expect raw NAL units. (Timestamps are big-endian
   `uint64`.)
4. **Media frames are expected to be PLAIN (unencrypted)** — the Vita should treat
   AV media frame payloads as raw bytes (no cryptor). Only the protobuf control/specific
   messages (open/setup/focus/ack/input) go through the TLS cryptor. Confirm by capture
   before finalizing the messenger (see unknowns).
5. **Audio without rendering**: implement an audio channel service that answers open,
   setup, and start/stop, ACKs every audio frame, and discards payload — no decoder, no
   output. This satisfies the phone and keeps the AA session alive while audio is played
   over car Bluetooth. Advertise the audio descriptors so the phone opens them.
6. **Video focus must be granted**: after video SETUP_RESPONSE, send
   `VIDEO_FOCUS_INDICATION(FOCUSED, unrequested=false)` proactively, and also reply to any
   `VIDEO_FOCUS_REQUEST` from the phone the same way. The `unrequested` field should be
   `false` for openauto-style behavior.
7. **Input**: advertise INPUT descriptor with `supported_keycodes` + `touch_screen_config`
   (Vita screen 960×544), answer BINDING_RESPONSE OK, then emit `INPUT_EVENT_INDICATION`
   for touch (PRESS/RELEASE/DRAG with x/y/pointer_id) and buttons.
8. **Session id bookkeeping**: store the phone-assigned `session` from START_INDICATION and
   echo it verbatim in every media ACK; reset to -1 on STOP.

## Remaining unknowns

- **Channel-open order** is phone-driven and not visible in aasdk/openauto. A live
  capture is needed to state a concrete order (e.g. whether the phone opens VIDEO before
  or after the audio channels, and whether it waits for each SETUP_RESPONSE before the
  next open). **Needs spike/capture.**
- **ENCRYPTED flag on media frames** is inferred (PLAIN) from the HU-side passthrough
  design and community reports, not directly observed in this codebase (the flag is set by
  the phone). **Confirm with a packet capture** before hard-coding the messenger's
  per-message encryption decision for AV media frames.
- **`config_index` handling**: openauto ignores the phone's `config_index` and replies
  `configs=[0]`, `max_unacked=1`. Whether the phone requires the HU to honor a specific
  config index (or whether `configs`/`config` values must be non-zero in some flows) is
  unverified. **Needs capture.**
- **Video STOP_INDICATION**: openauto's `VideoService` does not implement the stop
  handler (interface requires it). The exact expectation for video stop (session reset,
  decoder flush, re-focus) is unverified. **Needs spike.**
- **Whether audio descriptors are mandatory**: PSVitaAuto plans to negotiate audio but not
  render it; whether the phone proceeds correctly if audio descriptors are omitted (vs.
  present-but-discarded) is unverified. **Needs spike.**
- **Input binding**: the phone's `BINDING_REQUEST` scan-code set and whether it must be
  answered before any `INPUT_EVENT_INDICATION` is accepted are behaviorally known from
  openauto but not confirmed against a stock phone. **Needs capture.**

## Recommended next steps

1. Turn §1–§7 into a spec-level sequence diagram / state table for the `aa-media`
   feature spec (video + audio-negotiate + input).
2. Spike the raw-H.264 path on the Vita (or host) with a dummy decoder stub to confirm
   frame boundaries, timestamp endianness, and the PLAIN encryption assumption against a
   recorded AA session.
3. Spike audio "negotiate-but-discard": confirm the phone keeps streaming while the HU
   ACKs every frame without rendering (compare advertised-but-discarded vs. omitted
   descriptors).
4. Confirm channel-open ordering and `config_index` semantics with a packet capture of a
   real phone (tcpdump on the 5277 session), then reconcile with this note.
5. Update `specs/STATUS.md` / `BACKLOG.md` once the media-frame encryption flag and
   channel-open order are confirmed, and fold the input-channel flow into the `aa-input`
   feature spec.
