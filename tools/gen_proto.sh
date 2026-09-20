#!/bin/sh
# Regenerate the nanopb C bindings from the vendored modern aap_protobuf
# schemas (from f-io/LIVI, GPL-3.0-or-later).
# Requires: protoc (brew protobuf) + python3 protobuf/grpcio-tools.
set -e

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
NANOPB_GEN="$ROOT/third_party/nanopb/generator/protoc-gen-nanopb"
PROTO_ROOT="$ROOT/third_party"
OUT="$ROOT/src/core/aa_proto_generated"

rm -rf "$OUT"
mkdir -p "$OUT"

cd "$PROTO_ROOT"
protoc \
  --plugin=protoc-gen-nanopb="$NANOPB_GEN" \
  --nanopb_out="$OUT" \
  --nanopb_opt=-smax_size:128 \
  --nanopb_opt=-smax_count:16 \
  -I . \
  $(find aap_protobuf -name '*.proto' ! -path '*/channel/control/InstrumentClusterInput.proto')

echo "generated $(find "$OUT" -name '*.pb.c' | wc -l | tr -d ' ') .pb.c files"
