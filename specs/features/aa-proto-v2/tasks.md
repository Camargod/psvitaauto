# Tasks: aa-proto-v2

Spec: ../aa-proto-v2/spec.md
Plan: ../aa-proto-v2/plan.md
Updated: 2026-09-16

## T1: Vendor modern schemas + regenerate

- [x] done

DoD: `third_party/aap_protobuf/` (253 protos), `tools/gen_proto.sh` regenerates
nanopb C into `src/core/aa_proto_generated/`.

## T2: Rewrite message layer

- [x] done

DoD: `aa_constants.h` (modern channel IDs), `aa_msg.h` (message IDs), `control.c`
(service discovery, audio focus, channel open, media, sensor) against modern schemas.

## T3: Version + vehicle_type + required fields

- [x] done

DoD: version 1.7; `HeadUnitInfo.vehicle_type=3`; `margin_width`/`margin_height`;
`make/model/year/...` present.

## T4: Sensor handling

- [x] done

DoD: `SENSOR_MESSAGE_REQUEST` → `SENSOR_MESSAGE_RESPONSE`; `DrivingStatus` sent
on sensor channel open.

## T5: Mock + harness updated

- [x] done

DoD: `mock_phone.c`/`harness.c` use modern schemas; ctest passes; real-phone
stream validated.
