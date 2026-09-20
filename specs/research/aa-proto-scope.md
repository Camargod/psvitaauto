# Research: aa-proto-scope

Date: 2026-09-16
Question: how to implement the AA protobuf layer with nanopb from aasdk schemas

## Findings

### 1. aasdk proto inventory (f1xpl/aasdk, branch `development`, dir `aasdk_proto`)

- **96 `.proto` files** total (+1 `CMakeLists.txt` that is not a proto).
- **Syntax split: 9 proto2, 87 proto3.** All files carry an explicit `syntax`
  declaration (none rely on the legacy no-syntax = proto2 default).
- The 9 proto2 files are all "response/indication" messages that use `required`
  fields: `AVChannelSetupResponseMessage`, `AVInputOpenResponseMessage`,
  `AVMediaAckIndicationMessage`, `AuthCompleteIndicationMessage`,
  `BindingResponseMessage`, `ChannelOpenResponseMessage`, `DrivingStatusData`,
  `NightModeData`, `SensorStartResponseMessage`.
- **No `map`, `oneof`, `group`, `extensions`, or `[packed]`** usage anywhere in the
  schema set. 96 `message` declarations, 26 `enum` declarations (every enum is
  declared inside a wrapper message, e.g. `message Status { enum Enum { ... } }`).
- **Message-ID enum files** (define the per-channel IDs; each wraps the enum in a
  message so the ID type is `Xxx.Enum`):
  - `ControlMessageIdsEnum.proto` → `f1x.aasdk.proto.ids.ControlMessage.Enum`
  - `AVChannelMessageIdsEnum.proto` → `...ids.AVChannelMessage.Enum`
  - `InputChannelMessageIdsEnum.proto` → `...ids.InputChannelMessage.Enum`
  - `SensorChannelMessageIdsEnum.proto` → `...ids.SensorChannelMessage.Enum`
  - `BluetoothChannelMessageIdsEnum.proto` → `...ids.BluetoothChannelMessage.Enum`

### 2. nanopb (current stable)

- **Version:** `0.4.9.2` (Git tag `nanopb-0.4.9.2`, published 2026-08-25; latest
  non-prerelease at research time).
- **License:** zlib (permissive; commercial use and redistribution allowed with
  attribution). Confirmed from `LICENSE.txt`.
- **Generator:** `generator/nanopb_generator.py` (the real logic) plus the
  `generator/protoc-gen-nanopb` wrapper script, which is just
  `from nanopb_generator import *; main_plugin()`. It is driven by `protoc` as a
  plugin. nanopb also ships a bundled `protoc` binary and `proto/nanopb_pb2.py`
  under `generator/proto`, and needs the Python `protobuf` package at generation
  time (it force-sets `PROTOCOL_BUFFERS_PYTHON_IMPLEMENTATION=python`).
- **Invocation (confirmed from `protoc-gen-nanopb` header):**
  `protoc --plugin=protoc-gen-nanopb=..../protoc-gen-nanopb --nanopb_out=OUT_DIR file.proto`
- **proto2 + proto3:** nanopb supports both. proto2 `required`/`optional`/`default`
  are honored; proto3 implicit defaults are handled by generating a
  `Message_init_default` initializer macro. Packed repeated fields: nanopb encodes
  packed by default (matching proto3/libprotobuf); `PB_ENCODE_ARRAYS_UNPACKED`
  compile option opts out. `oneof` and `map` are supported (map → repeated
  key/value pairs) but **are irrelevant here because aasdk uses neither**.
- **Key caveat for aasdk schemas:** by default nanopb maps `string`/`bytes` (and
  repeated fields) to **callback functions** unless `max_size`/`max_length` is set
  (via `.options` file, `-s` CLI, or `(nanopb).max_size` annotation). aasdk has
  many `string` fields (e.g. `ServiceDiscoveryRequest.device_name/device_brand`,
  `ServiceDiscoveryResponse.head_unit_name/car_model/...`). These **must** be given
  explicit `max_size` (or intentionally left as callbacks) before generation,
  otherwise the generated C structs will be callback-based and awkward to fill.
- Other generator options of note: `-L/--library-include-format` (library include
  format), `--strip-path` (control `#include` path shape), `-s OPTION:VALUE`
  (set field/msg options from CLI), `-f/--options-file` (per-file options).

### 3. protoc availability on macOS

- `protoc` is **not currently installed** on this machine. Homebrew 7.0.1 is
  present; `brew info protobuf` reports stable `36.1` (alias `protobuf@36`,
  license BSD-3-Clause, dep: abseil).
- Install: `brew install protobuf` (provides `protoc`), plus `pip install protobuf`
  (and optionally `grpcio-tools`) for the nanopb generator's Python dependency.
  nanopb's bundled `protoc` under `generator/proto/` can be used as an alternative.

### 4. Wire framing — how the 2-byte message ID is attached (aasdk source)

Confirmed from `src/Messenger/Message.cpp`, `MessageId.cpp`, `MessageOutStream.cpp`,
`MessageInStream.cpp`, `FrameHeader.cpp`, `FrameSize.cpp`, `Transport.cpp`:

- The application-level message **payload** is built as:
  `<2-byte messageId><body>`, where the messageId is **BIG-ENDIAN**
  (`boost::endian::native_to_big` on write, `big_to_native` on read).
- The full **wire frame** is:
  - `FrameHeader` (2 bytes): byte0 = `channelId` (enum 0=CONTROL, 1=INPUT, 2=SENSOR,
    3=VIDEO, 4=MEDIA_AUDIO, 5=SPEECH_AUDIO, 6=SYSTEM_AUDIO, 7=AV_INPUT,
    8=BLUETOOTH); byte1 = flags (frameType bits 0-1, messageType bit 2 = 0x04
    "CONTROL", encryptionType bit 3 = 0x08 "ENCRYPTED").
  - `FrameSize` (2 bytes SHORT, or 6 bytes EXTENDED for the FIRST frame of a split
    message): **big-endian** (EXTENDED = 2-byte frame size + 4-byte total size).
  - `payload` = `<2-byte big-endian messageId><body>`, encrypted with the TLS
    session if the ENCRYPTED flag is set.
- **Max payload per frame = `0x4000` (16 KB)** (`cMaxFramePayloadSize` in
  `MessageOutStream.hpp`). Larger messages (e.g. a full H.264 frame on the video
  channel) are split across FIRST/MIDDLE/LAST frames and reassembled.
- **Not protobuf:** the following message bodies are raw binary, not protobuf:
  - `VERSION_REQUEST`/`VERSION_RESPONSE`: 2×uint16 BE (major, minor) + uint16 BE
    status. aasdk sends `AASDK_MAJOR=1`, `AASDK_MINOR=1` (`include/f1x/aasdk/Version.hpp`).
  - `SSL_HANDSHAKE`: raw TLS handshake bytes.
  - `AV_MEDIA_WITH_TIMESTAMP_INDICATION` / `AV_MEDIA_INDICATION`: raw codec
    (H.264) bitstream (with an optional 8-byte timestamp for the former).

### 5. Interop with the phone's libprotobuf

- nanopb emits standard protobuf wire format (varint / length-delimited / 32-bit /
  64-bit field encodings), so it is wire-compatible with the phone's
  `libprotobuf`. Field numbers and types in our vendored `.proto` must match the
  canonical aasdk schema **exactly** (names/packages do not affect the wire).
- Packed repeated fields match (proto3 default packed = nanopb default packed).
- proto3 implicit defaults are omitted on the wire in both, so round-trips agree.
- Unknown fields are skipped by nanopb's decoder by default, so extra fields from
  newer phone builds won't break parsing (and vice-versa).
- Caveats to watch: `string`/`bytes` require `max_size` (see §2); the `required`
  proto2 fields in the response messages must always be populated (nanopb does not
  enforce presence, but the phone's parser will reject a missing required field);
  large protobuf messages are avoided here because the big payloads (video/audio)
  are raw bytes, not protobuf.

### 6. Message-ID set for the head-unit control flow (exact values)

From the enum files (hex values are literal in the `.proto`):

**Control channel (`ControlMessage.Enum`, channelId 0):**
- `VERSION_REQUEST = 0x0001` (raw), `VERSION_RESPONSE = 0x0002` (raw)
- `SSL_HANDSHAKE = 0x0003` (raw TLS)
- `AUTH_COMPLETE = 0x0004` (protobuf `AuthCompleteIndication`)
- `SERVICE_DISCOVERY_REQUEST = 0x0005` (protobuf `ServiceDiscoveryRequest`)
- `SERVICE_DISCOVERY_RESPONSE = 0x0006` (protobuf `ServiceDiscoveryResponse`)
- `CHANNEL_OPEN_REQUEST = 0x0007` (protobuf `ChannelOpenRequest`)
- `CHANNEL_OPEN_RESPONSE = 0x0008` (protobuf `ChannelOpenResponse`)
- `PING_REQUEST = 0x000b` (protobuf `PingRequest`)
- `PING_RESPONSE = 0x000c` (protobuf `PingResponse`)
- (`0x000d`–`0x0013` = navigation/audio focus, shutdown, voice session — optional)

Note `CHANNEL_OPEN_REQUEST`/`CHANNEL_OPEN_RESPONSE` are `ControlMessage` IDs but are
sent on **each** channel (INPUT/VIDEO/AV_INPUT/audio) with the `messageType=CONTROL`
flag set.

**Input channel (`InputChannelMessage.Enum`, channelId 1):**
- `INPUT_EVENT_INDICATION = 0x8001` (protobuf `InputEventIndication`, HU→phone)
- `BINDING_REQUEST = 0x8002` (protobuf `BindingRequest`, phone→HU)
- `BINDING_RESPONSE = 0x8003` (protobuf `BindingResponse`, HU→phone)

**Video/AV channel (`AVChannelMessage.Enum`, channelId 3, also used by audio chans):**
- `AV_MEDIA_WITH_TIMESTAMP_INDICATION = 0x0000` (raw)
- `AV_MEDIA_INDICATION = 0x0001` (raw)
- `SETUP_REQUEST = 0x8000` (protobuf `AVChannelSetupRequest`, phone→HU)
- `START_INDICATION = 0x8001` (protobuf `AVChannelStartIndication`, phone→HU)
- `STOP_INDICATION = 0x8002` (protobuf `AVChannelStopIndication`, phone→HU)
- `SETUP_RESPONSE = 0x8003` (protobuf `AVChannelSetupResponse`, HU→phone)
- `AV_MEDIA_ACK_INDICATION = 0x8004` (protobuf `AVMediaAckIndication`, HU→phone)
- `AV_INPUT_OPEN_REQUEST = 0x8005` / `AV_INPUT_OPEN_RESPONSE = 0x8006` (protobuf, AV_INPUT channel)
- `VIDEO_FOCUS_REQUEST = 0x8007` (protobuf `VideoFocusRequest`, phone→HU)
- `VIDEO_FOCUS_INDICATION = 0x8008` (protobuf `VideoFocusIndication`, HU→phone)

### 7. Minimal proto closure for the head-unit role

The 17 seed messages needed for control + video + input are:
`AuthCompleteIndicationMessage`, `ServiceDiscoveryRequestMessage`,
`ServiceDiscoveryResponseMessage`, `ChannelOpenRequestMessage`,
`ChannelOpenResponseMessage`, `PingRequestMessage`, `PingResponseMessage`,
`InputEventIndicationMessage`, `BindingRequestMessage`, `BindingResponseMessage`,
`AVChannelSetupRequestMessage`, `AVChannelSetupResponseMessage`,
`AVChannelStartIndicationMessage`, `AVChannelStopIndicationMessage`,
`AVMediaAckIndicationMessage`, `VideoFocusIndicationMessage`,
`VideoFocusRequestMessage`.

Their **full transitive import closure = 49 files** (of 96). The dominant pull is
`ChannelDescriptorData` → `{SensorChannelData, AVChannelData, InputChannelData,
AVInputChannelData, BluetoothChannelData, NavigationChannelData,
VendorExtensionChannelData}` plus their enums/config sub-messages.

The 49-file closure is:
- IDs/control: `AuthCompleteIndicationMessage`, `ChannelOpenRequestMessage`,
  `ChannelOpenResponseMessage`, `PingRequestMessage`, `PingResponseMessage`,
  `ServiceDiscoveryRequestMessage`, `ServiceDiscoveryResponseMessage`, `StatusEnum`.
- Descriptors: `ChannelDescriptorData`, `SensorChannelData`, `SensorData`,
  `SensorTypeEnum`, `BluetoothChannelData`, `BluetoothPairingMethodEnum`,
  `NavigationChannelData`, `NavigationImageOptionsData`,
  `VendorExtensionChannelData`, `AVChannelData`, `InputChannelData`,
  `AVInputChannelData`, `TouchConfigData`, `AudioConfigData`, `VideoConfigData`.
- AV/input messages: `InputEventIndicationMessage`, `BindingRequestMessage`,
  `BindingResponseMessage`, `TouchEventData`, `TouchLocationData`, `TouchActionEnum`,
  `ButtonEventsData`, `ButtonEventData`, `AbsoluteInputEventsData`,
  `AbsoluteInputEventData`, `RelativeInputEventsData`, `RelativeInputEventData`,
  `AVChannelSetupRequestMessage`, `AVChannelSetupResponseMessage`,
  `AVChannelSetupStatusEnum`, `AVChannelStartIndicationMessage`,
  `AVChannelStopIndicationMessage`, `AVMediaAckIndicationMessage`,
  `VideoFocusIndicationMessage`, `VideoFocusRequestMessage`, `VideoFocusModeEnum`,
  `VideoFocusReasonEnum`, `AVStreamTypeEnum`, `AudioTypeEnum`,
  `VideoResolutionEnum`, `VideoFPSEnum`.

**Pruning option:** because the head unit only *encodes* `ChannelDescriptor` (it
never receives one), the sensor/bluetooth/navigation/vendor-extension descriptor
fields (and their imports) can be dropped from a curated `ChannelDescriptorData`,
reducing the closure to **~41 files**. Field numbers of the kept fields
(`channel_id=1`, `av_channel=3`, `input_channel=4`, `av_input_channel=5`) are
preserved, so wire compatibility is retained. This must be validated by a spike
(see unknowns).

## Sources (URLs)

- aasdk proto dir listing: https://api.github.com/repos/f1xpl/aasdk/contents/aasdk_proto?ref=development
- aasdk repo (browse): https://github.com/f1xpl/aasdk/tree/development/aasdk_proto
- ControlMessageIdsEnum.proto: https://github.com/f1xpl/aasdk/blob/development/aasdk_proto/ControlMessageIdsEnum.proto
- AVChannelMessageIdsEnum.proto: https://github.com/f1xpl/aasdk/blob/development/aasdk_proto/AVChannelMessageIdsEnum.proto
- InputChannelMessageIdsEnum.proto: https://github.com/f1xpl/aasdk/blob/development/aasdk_proto/InputChannelMessageIdsEnum.proto
- SensorChannelMessageIdsEnum.proto: https://github.com/f1xpl/aasdk/blob/development/aasdk_proto/SensorChannelMessageIdsEnum.proto
- Framing source: https://github.com/f1xpl/aasdk/tree/development/src/Messenger (Message.cpp, MessageId.cpp, FrameHeader.cpp, FrameSize.cpp, MessageOutStream.cpp, MessageInStream.cpp)
- Control flow source: https://github.com/f1xpl/aasdk/blob/development/src/Channel/Control/ControlServiceChannel.cpp
- Input flow source: https://github.com/f1xpl/aasdk/blob/development/src/Channel/Input/InputServiceChannel.cpp
- Video flow source: https://github.com/f1xpl/aasdk/blob/development/src/Channel/AV/VideoServiceChannel.cpp
- aasdk Version.hpp: https://github.com/f1xpl/aasdk/blob/development/include/f1x/aasdk/Version.hpp
- nanopb repo: https://github.com/nanopb/nanopb
- nanopb release 0.4.9.2: https://github.com/nanopb/nanopb/releases/tag/nanopb-0.4.9.2
- nanopb LICENSE: https://github.com/nanopb/nanopb/blob/master/LICENSE.txt
- nanopb generator: https://github.com/nanopb/nanopb/tree/master/generator (nanopb_generator.py, protoc-gen-nanopb)
- nanopb concepts (proto2/3, packed, defaults, oneof): https://github.com/nanopb/nanopb/blob/master/docs/concepts.md
- nanopb reference (options, -L, max_size, packed): https://github.com/nanopb/nanopb/blob/master/docs/reference.md
- Homebrew protobuf formula: https://formulae.brew.sh/formula/protobuf (stable 36.1)

## Implications for PSVitaAuto

- nanopb is the right generator: permissive zlib license, single `.pb.c`/`.pb.h`
  per message, no dynamic allocation required, works on C11 (the runtime is plain
  C: `pb_common.c`, `pb_decode.c`, `pb_encode.c` + `pb.h`). No C++/STL needed on
  the Vita side.
- The protobuf layer is only needed for **control-plane and input-plane** messages.
  Version handshake, SSL handshake, and video/audio frame bodies are raw bytes and
  are handled by the framing layer (`aa-framing`), not nanopb.
- Message IDs must be emitted/parsed as **big-endian uint16**, not the little-endian
  assumption some AA write-ups use; aasdk's `native_to_big`/`big_to_native` is the
  proven-against-real-phones reference.
- A single `ChannelDescriptorData` import pulls in ~49 files transitively, so
  vendoring strategy matters for both build cleanliness and Vita-side code size.
- Every `string` field (device name/brand, head unit name/model/year/serial/build,
  etc.) needs an explicit `max_size` in a nanopb `.options` file before generation.

## Remaining unknowns

1. **Exact minimum channel-descriptor set the phone requires** to actually start
   video: whether a `MediaAudio`/`SpeechAudio`/`SystemAudio` `AVChannel` descriptor
   must be present in `ServiceDiscoveryResponse` (project stance is "negotiate but
   don't output"), and whether the phone rejects a response that omits
   sensor/nav/vendor descriptors. This determines pruned-41 vs full-49 (or a custom
   in-between) closure.
2. **Endianness cross-check** against an independent implementation (OpenAuto /
   Headunit Reloaded behavior): aasdk is big-endian and is the working reference,
   but confirming there is no phone firmware that expects little-endian on the
   version/handshake path is worth a spike before freezing the framing code.
3. **`max_size` values** for the string fields (what the phone tolerates vs. what we
   must allocate) — needs values chosen (e.g. 128/256) and validated against real
   device responses.
4. Whether generated `.pb.c/.pb.h` should be committed to the repo or produced at
   build time (depends on whether protoc is guaranteed on the CI/build host).
5. nanopb `PB_ENCODE_ARRAYS_UNPACKED` vs packed: aasdk `repeated` fields are all
   scalar and proto3 (packed by default) — likely fine, but the `repeated int32
   scan_codes` in `BindingRequest` and `repeated uint32 supported_keycodes` should
   be verified against what the phone sends/expects.

## Recommended next steps

1. Spike (prototype): vendor the full `aasdk_proto` verbatim (git submodule pinned
   to `f1xpl/aasdk@development` or a frozen copy) under `third_party/aasdk_proto/`,
   and vendor nanopb `0.4.9.2` (generator + runtime `pb_*.c/pb.h`) under
   `third_party/nanopb/`.
2. Install `brew install protobuf` + `pip install protobuf grpcio-tools`; write a
   `tools/gen_proto.sh` that runs protoc with `--plugin=protoc-gen-nanopb` over the
   curated 49-file list, plus a `aa.options` file setting `max_size` on all string
   fields. Emit into `src/aa-proto/generated/`.
3. Decide pruned vs full closure after the descriptor-set spike (unknown #1). Start
   with the full 49-file closure to match aasdk exactly, then prune if Vita code
   size or build time becomes a concern.
4. Prototype the control-flow messages end-to-end against a real phone: version
   request (raw binary) → SSL handshake (raw) → auth complete → service discovery →
   channel open (video/input) → ping, then send a synthetic `InputEventIndication`
   (touch) and confirm the phone reacts. This validates message IDs, endianness,
   and the `max_size` string values before any spec is written.
5. Record the confirmed endianness and descriptor-set results back into this note
   (or a follow-up spike report) so the `aa-proto` feature spec can rely on facts,
   not hypotheses.
