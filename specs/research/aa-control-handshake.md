# Research: aa-control-handshake

Date: 2026-09-16
Question: exact AA control-channel handshake flow for a head-unit client

## Findings

All findings are **facts read from `f1xpl/aasdk` (branch `development`) and
`f1xpl/openauto` (branch `development`)**, cited per-file. Hypotheses are
labelled explicitly. This note focuses on the *control-channel handshake state
machine* (the orchestration layer). The underlying wire framing, TLS
record-layer mechanics, and byte-level details are already covered in
`specs/research/aa-framing.md` and `specs/research/aa-tls-mechanics.md`; this
note cross-references them rather than duplicating.

### Role recap (who is the "client")

- The head unit (HU) is the **TCP client**: it connects to the phone on **TCP
  port 5277** (`openauto/src/autoapp/UI/ConnectDialog.cpp`, already documented in
  `aa-framing.md` §6).
- The HU is also the **TLS client** (`SSL_set_connect_state`, `TLS_client_method`)
  and the **protocol initiator** (it sends `VERSION_REQUEST` first).
- The HU is the **channel server** conceptually: it advertises channel descriptors
  in `SERVICE_DISCOVERY_RESPONSE`; the **phone** then sends `CHANNEL_OPEN_REQUEST`
  to open each channel (video/input/audio), and the HU answers with
  `CHANNEL_OPEN_RESPONSE`.

### 1. Version handshake — exact byte layout

`include/f1x/aasdk/Version.hpp`:
```cpp
static const uint16_t AASDK_MAJOR = 1;
static const uint16_t AASDK_MINOR = 1;
```

`ControlServiceChannel::sendVersionRequest`
(`src/Channel/Control/ControlServiceChannel.cpp`):

```cpp
auto message(std::make_shared<messenger::Message>(channelId_, messenger::EncryptionType::PLAIN, messenger::MessageType::SPECIFIC));
message->insertPayload(messenger::MessageId(proto::ids::ControlMessage::VERSION_REQUEST).getData()); // 0x0001

common::Data versionBuffer(4, 0);
reinterpret_cast<uint16_t&>(versionBuffer[0]) = boost::endian::native_to_big(AASDK_MAJOR); // 1
reinterpret_cast<uint16_t&>(versionBuffer[2]) = boost::endian::native_to_big(AASDK_MINOR); // 1
message->insertPayload(versionBuffer);
```

**VERSION_REQUEST body = 4 bytes, big-endian**:
```
uint16 major  (BE) = 0x0001   -> bytes 00 01
uint16 minor  (BE) = 0x0001   -> bytes 00 01
```
Full payload on the wire (after the 2-byte BE `MessageId` `00 01`) is
`00 01 00 01`. (The `MessageId` prefix is 2 bytes big-endian; see
`src/Messenger/MessageId.cpp` and `aa-framing.md`.)

`ControlServiceChannel::handleVersionResponse` (same file) parses the reply:

```cpp
const uint16_t* versionResponse = reinterpret_cast<const uint16_t*>(payload.cdata);
uint16_t majorCode = ... big_to_native(versionResponse[0]); // uint16 BE
uint16_t minorCode = ... big_to_native(versionResponse[1]); // uint16 BE
Status status = ... static_cast<Status>(versionResponse[2]); // uint16 BE
```

**VERSION_RESPONSE body = 6 bytes, big-endian**: `uint16 major`, `uint16 minor`,
`uint16 status`. Status enum (`aasdk_proto/VersionResponseStatusEnum.proto`):
`MATCH = 0`, `MISMATCH = 0xFFFF`. The HU expects `MATCH`; on `MISMATCH` it aborts
(`AndroidAutoEntity::onVersionResponse`).

Version response is dispatched to `onVersionResponse(majorCode, minorCode, status)`
(`IControlServiceChannelEventHandler.hpp`).

### 2. Handshake order (head-unit client) — exact sequence

Wire transport is established first (TCP connect to phone:5277). Then, on the
**CONTROL channel (channel id 0)**, the state machine is:

| # | Message (id) | Direction | Encryption | MessageType | State transition |
|---|---|---|---|---|---|
| 1 | VERSION_REQUEST (0x0001) | HU → phone | PLAIN | SPECIFIC | sent at `AndroidAutoEntity::start()` |
| 2 | VERSION_RESPONSE (0x0002) | phone → HU | PLAIN | SPECIFIC | `onVersionResponse` → if MATCH begin TLS |
| 3 | SSL_HANDSHAKE (0x0003) ×N | both | PLAIN | SPECIFIC | TLS record exchange loop (see §4) |
| 4 | AUTH_COMPLETE (0x0004) | HU → phone | PLAIN | SPECIFIC | sent when TLS completes |
| 5 | SERVICE_DISCOVERY_REQUEST (0x0005) | phone → HU | ENCRYPTED | SPECIFIC | `onServiceDiscoveryRequest` |
| 6 | SERVICE_DISCOVERY_RESPONSE (0x0006) | HU → phone | ENCRYPTED | SPECIFIC | HU advertises its channels |
| 7 | CHANNEL_OPEN_REQUEST (0x0007) | phone → HU | ENCRYPTED | CONTROL | per-channel `onChannelOpenRequest` |
| 8 | CHANNEL_OPEN_RESPONSE (0x0008) | HU → phone | ENCRYPTED | CONTROL | per-channel response |
| 9 | (per-channel AV setup/media/input) | both | ENCRYPTED | SPECIFIC | post-open, out of this note's scope |
| 10 | PING_REQUEST (0x000b) | HU → phone | PLAIN | SPECIFIC | every 5 s keepalive |
| 11 | PING_RESPONSE (0x000c) | phone → HU | PLAIN | SPECIFIC | `onPingResponse` → `pong()` |

Orchestration source: `openauto/src/autoapp/Service/AndroidAutoEntity.cpp`
(methods `start`, `onVersionResponse`, `onHandshake`, `onServiceDiscoveryRequest`,
`onPingResponse`, `schedulePing`, `sendPing`). Message-id enum:
`aasdk_proto/ControlMessageIdsEnum.proto`.

Note: `AndroidAutoEntityServiceImpl.cpp` does **not** exist in this codebase
(the 404 is expected); the orchestration lives in `AndroidAutoEntity.cpp`.

### 3. Encryption map (per message) — confirmed

From `ControlServiceChannel.cpp` (HU send side), `VideoServiceChannel.cpp` /
`AVInputServiceChannel.cpp` (channel-open), and `AndroidAutoEntity.cpp` (receive
side). Each `Message` is constructed with an explicit `EncryptionType`.

**PLAIN** (all `MessageType::SPECIFIC`, on CONTROL channel):
- VERSION_REQUEST / VERSION_RESPONSE (0x0001 / 0x0002)
- SSL_HANDSHAKE (0x0003)
- **AUTH_COMPLETE (0x0004) — confirmed PLAIN**
- PING_REQUEST / PING_RESPONSE (0x000b / 0x000c)

**ENCRYPTED**:
- **SERVICE_DISCOVERY_REQUEST / SERVICE_DISCOVERY_RESPONSE (0x0005 / 0x0006) — confirmed ENCRYPTED** (`sendServiceDiscoveryResponse` constructs `EncryptionType::ENCRYPTED`)
- CHANNEL_OPEN_REQUEST / CHANNEL_OPEN_RESPONSE (0x0007 / 0x0008) — ENCRYPTED, `MessageType::CONTROL`
- AUDIO_FOCUS_* , SHUTDOWN_* , NAVIGATION_FOCUS_* (post-auth control messages) — ENCRYPTED
- All per-channel setup/ack/input messages — ENCRYPTED

This matches the `aa-tls-mechanics.md` map exactly. **Confirmed: AUTH_COMPLETE is
PLAIN, SERVICE_DISCOVERY is ENCRYPTED.**

### 4. SSL_HANDSHAKE driving — the interleaving loop

The TLS handshake is driven through `Cryptor` (`aasdk/src/Messenger/Cryptor.cpp`)
interleaved with `sendHandshake`/`onHandshake`. Exact loop from
`AndroidAutoEntity.cpp`:

```cpp
void onVersionResponse(major, minor, status) {
    if (status == MISMATCH) { quit(); }
    else {
        cryptor_->doHandshake();                                   // SSL_do_handshake → SSL_ERROR_WANT_READ (returns false)
        controlServiceChannel_->sendHandshake(cryptor_->readHandshakeBuffer(), ...); // send ClientHello
        controlServiceChannel_->receive(this);                     // arm for next SSL_HANDSHAKE
    }
}

void onHandshake(payload) {
    cryptor_->writeHandshakeBuffer(payload);                       // feed inbound TLS records → rbio
    if (!cryptor_->doHandshake()) {                                // still SSL_ERROR_WANT_READ
        controlServiceChannel_->sendHandshake(cryptor_->readHandshakeBuffer(), ...); // drain wbio → send more records
    } else {                                                       // SSL_ERROR_NONE
        AuthCompleteIndication auth; auth.set_status(Status::OK);
        controlServiceChannel_->sendAuthComplete(auth, ...);       // TLS done → AUTH_COMPLETE
    }
    controlServiceChannel_->receive(this);                         // always re-arm
}
```

`Cryptor::doHandshake` (`Cryptor.cpp`) returns:
- `false` on `SSL_ERROR_WANT_READ` (needs more peer records),
- `true` on `SSL_ERROR_NONE` (sets `isActive_ = true`),
- throws on any other error.

`sendHandshake` (`ControlServiceChannel.cpp`) wraps the TLS record bytes in a
PLAIN `SSL_HANDSHAKE` (0x0003) `SPECIFIC` message on the CONTROL channel.

**How the HU knows the phone is done:** it does **not** wait for a dedicated
"handshake finished" message. It loops: each inbound `SSL_HANDSHAKE` payload is
pushed into the SSL read BIO, then `SSL_do_handshake` is re-run; when
`SSL_do_handshake` returns `SSL_ERROR_NONE`, the handshake is complete and the HU
sends `AUTH_COMPLETE`. (Because the HU is the TLS *client*, `SSL_do_handshake`
returns NONE only after it has received the phone's `Finished` — the phone's side
of the handshake is implicitly "done".)

### 5. AUTH_COMPLETE

`aasdk_proto/AuthCompleteIndicationMessage.proto` (proto2):
```proto
message AuthCompleteIndication { required enums.Status.Enum status = 1; }
```
`Status` (`aasdk_proto/StatusEnum.proto`): `OK = 0`, `FAIL = 1`.

The HU sends `AUTH_COMPLETE` (PLAIN, SPECIFIC, CONTROL channel) with `status = OK`
(`AndroidAutoEntity::onHandshake`). It carries **only** the status field — no
token, no cert data (auth/identity was already established by the mutual-TLS
client cert during the handshake).

### 6. SERVICE_DISCOVERY

`aasdk_proto/ServiceDiscoveryRequestMessage.proto` (proto3), sent by the phone:
```proto
message ServiceDiscoveryRequest {
    string device_name = 4;   // phone's name
    string device_brand = 5;  // phone's brand
}
```

`aasdk_proto/ServiceDiscoveryResponseMessage.proto` (proto3), sent by the HU:
```proto
message ServiceDiscoveryResponse {
    repeated data.ChannelDescriptor channels = 1;
    string head_unit_name = 2;  // e.g. "OpenAuto"
    string car_model = 3;
    string car_year = 4;
    string car_serial = 5;
    bool left_hand_drive_vehicle = 6;
    string headunit_manufacturer = 7;
    string headunit_model = 8;
    string sw_build = 9;
    string sw_version = 10;
    bool can_play_native_media_during_vr = 11;
    bool hide_clock = 12;
}
```

`aasdk_proto/ChannelDescriptorData.proto` (proto3) — the descriptor layout:
```proto
message ChannelDescriptor {
    uint32 channel_id = 1;
    SensorChannel sensor_channel = 2;
    AVChannel av_channel = 3;
    InputChannel input_channel = 4;
    AVInputChannel av_input_channel = 5;
    BluetoothChannel bluetooth_channel = 6;
    NavigationChannel navigation_channel = 8;
    VendorExtensionChannel vendor_extension_channel = 12;
}
```

`channel_id` is a `uint32` set to the same numeric `ChannelId` used in the frame
header (`static_cast<uint32_t>(channel_->getId())` — see §7). Exactly one of the
sub-messages (`av_channel`, `input_channel`, etc.) is populated per descriptor.

**To receive video, the HU MUST advertise a descriptor with `channel_id = 3`
(`ChannelId::VIDEO`) and a populated `av_channel`.** From
`openauto/src/autoapp/Service/VideoService.cpp` `fillFeatures`:

```cpp
auto* channelDescriptor = response.add_channels();
channelDescriptor->set_channel_id(static_cast<uint32_t>(channel_->getId())); // VIDEO = 3
auto* videoChannel = channelDescriptor->mutable_av_channel();
videoChannel->set_stream_type(aasdk::proto::enums::AVStreamType::VIDEO);     // = 3
videoChannel->set_available_while_in_call(true);
auto* videoConfig1 = videoChannel->add_video_configs();
videoConfig1->set_video_resolution(videoOutput_->getVideoResolution());       // enum
videoConfig1->set_video_fps(videoOutput_->getVideoFPS());                     // enum
videoConfig1->set_margin_height(...); videoConfig1->set_margin_width(...); videoConfig1->set_dpi(...);
```

Supporting field layouts:
- `AVChannel` (`AVChannelData.proto`): `stream_type` (1), `audio_type` (2),
  `audio_configs` (3), `video_configs` (4), `available_while_in_call` (5).
- `VideoConfig` (`VideoConfigData.proto`): `video_resolution` (1), `video_fps` (2),
  `margin_width` (3), `margin_height` (4), `dpi` (5), `additional_depth` (6).
- `AVStreamType` (`AVStreamTypeEnum.proto`): `NONE=0, AUDIO=1, VIDEO=3`.
- `VideoResolution` (`VideoResolutionEnum.proto`): `NONE=0, _480p=1, _720p=2, _1080p=3`.
- `VideoFPS` (`VideoFPSEnum.proto`): `NONE=0, _30=1, _60=2`.

For input, the HU also advertises (if it wants touch/button input forwarded to
the phone) a descriptor with `channel_id = 1` (`ChannelId::INPUT`) and a populated
`input_channel` (`InputService::fillFeatures`): `supported_keycodes` (repeated
uint32) and `touch_screen_config` (`TouchConfig { width, height }`). The full
openauto service list (audio input, media/system/speech audio, sensor, video,
bluetooth, input) is created in `ServiceFactory.cpp`; each `IService::fillFeatures`
appends its descriptor(s).

### 7. CHANNEL_OPEN

`aasdk_proto/ChannelOpenRequestMessage.proto` (proto3), sent by the **phone**:
```proto
message ChannelOpenRequest {
    int32 priority = 1;
    int32 channel_id = 2;
}
```
`aasdk_proto/ChannelOpenResponseMessage.proto` (proto2), sent by the **HU**:
```proto
message ChannelOpenResponse { required enums.Status.Enum status = 1; }  // OK=0, FAIL=1
```

Framing of both (from `VideoServiceChannel::sendChannelOpenResponse` and the
`handleChannelOpenRequest` dispatch in `VideoServiceChannel.cpp` /
`AVInputServiceChannel.cpp`):

- **MessageType = CONTROL** (bit 0x04 in the frame-header flags byte) — this is
  the distinguishing mark of a channel-open control message vs a channel-specific
  (SPECIFIC) message.
- **EncryptionType = ENCRYPTED**.
- **Frame-header channel id = the target channel** (e.g. `ChannelId::VIDEO = 3`,
  `ChannelId::INPUT = 1`, `ChannelId::AV_INPUT = 7`), *not* CONTROL(0).
- Message id = 0x0007 (request) / 0x0008 (response).

So `CHANNEL_OPEN_REQUEST/RESPONSE` are the only control-plane messages carried on
a **non-control channel**, and they are the only ones using `MessageType::CONTROL`
in the handshake phase. The HU's `ControlServiceChannel` does **not** handle
`CHANNEL_OPEN_REQUEST`; each service channel handles its own.

The HU reads `request.priority()` for logging but the actual open/deny decision is
`Status::OK`/`FAIL` based on the local resource (e.g. `videoOutput_->open()`).

### 8. PING

`aasdk_proto/PingRequestMessage.proto` (proto3): `int64 timestamp = 1;`
`aasdk_proto/PingResponseMessage.proto` (proto3): `int64 timestamp = 1;`

openauto sends `PingRequest` with no timestamp set (default `0`); the phone echoes
the timestamp in `PingResponse`.

**Cadence**: `Pinger(ioService, 5000)` — 5000 ms — constructed in
`openauto/src/autoapp/Service/AndroidAutoEntityFactory.cpp`. Flow
(`AndroidAutoEntity.cpp`):
- `start()` → `schedulePing()`.
- `schedulePing()` arms a 5 s deadline timer; on expiry it calls `sendPing()` then
  `schedulePing()` again (continuous 5 s keepalive).
- `sendPing()` → `sendPingRequest` (PLAIN, SPECIFIC, CONTROL).
- `onPingResponse()` → `pinger_->pong()`.
- Deadlock detection (`Pinger::onTimerExceeded`): if `pingsCount - pongsCount > 1`
  at timer expiry → error → `triggerQuit()` (connection considered dead).

So the keepalive is: **HU pings every 5 s; the phone replies; >1 outstanding ping
⇒ connection failure.**

## Sources (URLs)

aasdk (`f1xpl/aasdk`, branch `development`):
- https://raw.githubusercontent.com/f1xpl/aasdk/development/src/Channel/Control/ControlServiceChannel.cpp
- https://raw.githubusercontent.com/f1xpl/aasdk/development/include/f1x/aasdk/Channel/Control/IControlServiceChannelEventHandler.hpp
- https://raw.githubusercontent.com/f1xpl/aasdk/development/src/Channel/AV/AVInputServiceChannel.cpp
- https://raw.githubusercontent.com/f1xpl/aasdk/development/src/Channel/AV/VideoServiceChannel.cpp
- https://raw.githubusercontent.com/f1xpl/aasdk/development/include/f1x/aasdk/Channel/AV/VideoServiceChannel.hpp
- https://raw.githubusercontent.com/f1xpl/aasdk/development/src/Channel/ServiceChannel.cpp
- https://raw.githubusercontent.com/f1xpl/aasdk/development/include/f1x/aasdk/Channel/ServiceChannel.hpp
- https://raw.githubusercontent.com/f1xpl/aasdk/development/src/Messenger/Messenger.cpp
- https://raw.githubusercontent.com/f1xpl/aasdk/development/src/Messenger/MessageOutStream.cpp
- https://raw.githubusercontent.com/f1xpl/aasdk/development/src/Messenger/MessageInStream.cpp
- https://raw.githubusercontent.com/f1xpl/aasdk/development/src/Messenger/FrameHeader.cpp
- https://raw.githubusercontent.com/f1xpl/aasdk/development/src/Messenger/FrameSize.cpp
- https://raw.githubusercontent.com/f1xpl/aasdk/development/src/Messenger/MessageId.cpp
- https://raw.githubusercontent.com/f1xpl/aasdk/development/src/Messenger/Cryptor.cpp
- https://raw.githubusercontent.com/f1xpl/aasdk/development/src/Transport/SSLWrapper.cpp
- https://raw.githubusercontent.com/f1xpl/aasdk/development/include/f1x/aasdk/Version.hpp
- https://raw.githubusercontent.com/f1xpl/aasdk/development/include/f1x/aasdk/Messenger/ChannelId.hpp
- https://raw.githubusercontent.com/f1xpl/aasdk/development/include/f1x/aasdk/Messenger/EncryptionType.hpp
- https://raw.githubusercontent.com/f1xpl/aasdk/development/include/f1x/aasdk/Messenger/MessageType.hpp
- https://raw.githubusercontent.com/f1xpl/aasdk/development/include/f1x/aasdk/Messenger/FrameType.hpp
- https://raw.githubusercontent.com/f1xpl/aasdk/development/aasdk_proto/ControlMessageIdsEnum.proto
- https://raw.githubusercontent.com/f1xpl/aasdk/development/aasdk_proto/VersionResponseStatusEnum.proto
- https://raw.githubusercontent.com/f1xpl/aasdk/development/aasdk_proto/StatusEnum.proto
- https://raw.githubusercontent.com/f1xpl/aasdk/development/aasdk_proto/AuthCompleteIndicationMessage.proto
- https://raw.githubusercontent.com/f1xpl/aasdk/development/aasdk_proto/ServiceDiscoveryRequestMessage.proto
- https://raw.githubusercontent.com/f1xpl/aasdk/development/aasdk_proto/ServiceDiscoveryResponseMessage.proto
- https://raw.githubusercontent.com/f1xpl/aasdk/development/aasdk_proto/ChannelDescriptorData.proto
- https://raw.githubusercontent.com/f1xpl/aasdk/development/aasdk_proto/AVChannelData.proto
- https://raw.githubusercontent.com/f1xpl/aasdk/development/aasdk_proto/VideoConfigData.proto
- https://raw.githubusercontent.com/f1xpl/aasdk/development/aasdk_proto/AVStreamTypeEnum.proto
- https://raw.githubusercontent.com/f1xpl/aasdk/development/aasdk_proto/VideoResolutionEnum.proto
- https://raw.githubusercontent.com/f1xpl/aasdk/development/aasdk_proto/VideoFPSEnum.proto
- https://raw.githubusercontent.com/f1xpl/aasdk/development/aasdk_proto/InputChannelData.proto
- https://raw.githubusercontent.com/f1xpl/aasdk/development/aasdk_proto/AVInputChannelData.proto
- https://raw.githubusercontent.com/f1xpl/aasdk/development/aasdk_proto/TouchConfigData.proto
- https://raw.githubusercontent.com/f1xpl/aasdk/development/aasdk_proto/ChannelOpenRequestMessage.proto
- https://raw.githubusercontent.com/f1xpl/aasdk/development/aasdk_proto/ChannelOpenResponseMessage.proto
- https://raw.githubusercontent.com/f1xpl/aasdk/development/aasdk_proto/PingRequestMessage.proto
- https://raw.githubusercontent.com/f1xpl/aasdk/development/aasdk_proto/PingResponseMessage.proto

openauto (`f1xpl/openauto`, branch `development`):
- https://raw.githubusercontent.com/f1xpl/openauto/development/src/autoapp/Service/AndroidAutoEntity.cpp
- https://raw.githubusercontent.com/f1xpl/openauto/development/src/autoapp/Service/AndroidAutoEntityFactory.cpp
- https://raw.githubusercontent.com/f1xpl/openauto/development/src/autoapp/Service/Pinger.cpp
- https://raw.githubusercontent.com/f1xpl/openauto/development/include/f1x/openauto/autoapp/Service/Pinger.hpp
- https://raw.githubusercontent.com/f1xpl/openauto/development/src/autoapp/Service/ServiceFactory.cpp
- https://raw.githubusercontent.com/f1xpl/openauto/development/src/autoapp/Service/VideoService.cpp
- https://raw.githubusercontent.com/f1xpl/openauto/development/src/autoapp/Service/InputService.cpp
- https://raw.githubusercontent.com/f1xpl/openauto/development/src/autoapp/Service/AudioService.cpp
- https://raw.githubusercontent.com/f1xpl/openauto/development/include/f1x/openauto/autoapp/Service/IService.hpp

Local cross-references:
- `specs/research/aa-framing.md` (wire framing, TCP 5277, TLS role)
- `specs/research/aa-tls-mechanics.md` (TLS record layer, cryptor, mbedTLS mapping)

## Implications for PSVitaAuto

1. **State machine is small and deterministic.** The entire control-channel flow
   is ~6 states (version → handshake → auth → discovery → channel-open → ping),
   all on `ChannelId::CONTROL` except channel-open (on the target channel). This
   is straightforward to encode as an explicit C state machine on the Vita.
2. **Version is fixed**: send `00 01 00 01` (major=1, minor=1) and expect a 6-byte
   BE reply with `status == MATCH (0)`. No negotiation of other versions.
3. **Encryption decision is per-message and static**: the pre-auth messages
   (VERSION, SSL_HANDSHAKE, AUTH_COMPLETE, PING) are always PLAIN; everything after
   AUTH_COMPLETE except PING is ENCRYPTED. This must be hard-coded in the messenger
   (the TLS record layer is per-frame; see `aa-tls-mechanics.md`).
4. **TLS must be driven by the SSL engine's return value, not by a fixed message
   count.** Reuse the exact loop: feed inbound `SSL_HANDSHAKE` → `mbedtls_ssl_handshake`
   → drain outbound records → send; when the handshake call reports success, send
   AUTH_COMPLETE. The mbedTLS equivalent is already mapped in `aa-tls-mechanics.md` §6.
5. **The Vita must advertise a VIDEO descriptor (channel_id=3, `av_channel` with
   `stream_type=VIDEO`, at least one `video_config` with resolution/fps/dpi/margins)
   to receive video**, plus an INPUT descriptor (channel_id=1) to forward touch.
   Audio descriptors are negotiated but not output (per project charter).
6. **Channel-open is phone-initiated**: the Vita only answers `CHANNEL_OPEN_RESPONSE`
   (status OK/FAIL) with `MessageType::CONTROL`, ENCRYPTED, on the target channel.
7. **Keepalive**: ping every 5 s; treat >1 outstanding ping as a dead link and
   teardown.

## Remaining unknowns

- **Channel-id values vs community docs.** aasdk uses `VIDEO=3, INPUT=1, SENSOR=2`
  (from the `ChannelId` enum's implicit numbering). Some reverse-engineered AA
  docs list different numeric channel ids (e.g. video `4`). aasdk/openauto is a
  known-working HU, so `3/1/2/...` must be what its supported AA phones accept,
  but this is **not independently confirmed** against a live phone. **Needs a spike
  / packet capture.**
- **Phone-side acceptance of the client cert / TLS parameters** is not visible in
  this codebase (aasdk sets `SSL_VERIFY_NONE` on the HU side only). Whether the
  phone verifies the "Google Automotive Link" client cert and exact cipher list is
  unverified (already flagged in `aa-framing.md` and `aa-tls-mechanics.md`).
- **`CHANNEL_OPEN_REQUEST` routing** is an aasdk implementation choice (carried on
  the target channel with `MessageType::CONTROL`). Whether a stock phone always
  does exactly this, or whether some flows route channel-open through the CONTROL
  channel, is unverified. **Needs capture.**
- **ServiceDiscoveryRequest/Response field requirements**: which of the response
  string fields (`head_unit_name`, `car_model`, etc.) the phone strictly requires
  vs treats as optional is not determined from source. aasdk fills them all.
- **`AndroidAutoEntityServiceImpl.cpp`** does not exist in this branch; if a later
  openauto refactor moved the orchestration there, the flow may differ slightly.
  (Confirmed against `development`.)

## Recommended next steps

1. Encode the state machine as a spec-level sequence diagram (the 11-step table in
   §2) and turn it into the `aa-control`/`aa-transport` feature spec's reference
   state list.
2. Spike against a real phone or a recorded capture to **confirm the channel-id
   numeric values** (esp. VIDEO=3) and the **channel-open routing** (target channel
   vs control channel).
3. Reuse `aa-tls-mechanics.md`'s mbedTLS mapping for the handshake loop and
   `aa-framing.md`'s byte layout for the messenger; do not re-derive them here.
4. Decide the minimal descriptor set (VIDEO + INPUT only, audio negotiated-but-not-output)
   and lock the `ServiceDiscoveryResponse` fields to fill.
5. Update `specs/STATUS.md` / `BACKLOG.md` once the channel-id and channel-open
   unknowns are resolved.
