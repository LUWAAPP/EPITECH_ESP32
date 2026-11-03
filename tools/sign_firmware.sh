#!/usr/bin/env bash
# Sign the built firmware image with the local private key.
#
#   tools/sign_firmware.sh                  signs the default build
#   tools/sign_firmware.sh path/to/bin sig  custom paths
#
# Produces firmware.sig next to the binary. Host it together with the binary and
# point cmd/ota at both URLs.
set -euo pipefail

ROOT=$(git rev-parse --show-toplevel 2>/dev/null || pwd)
cd "$ROOT"

BIN=${1:-.pio/build/ttgo-t7-v14-mini32/firmware.bin}
SIG=${2:-$(dirname "$BIN")/firmware.sig}
KEY=${PRIVATE_KEY:-private_key.pem}

[ -f "$BIN" ] || { echo "missing firmware: $BIN (run pio run first)" >&2; exit 1; }
[ -f "$KEY" ] || { echo "missing private key: $KEY (run tools/gen_keys.sh)" >&2; exit 1; }

openssl dgst -sha256 -sign "$KEY" -out "$SIG" "$BIN"
echo "signed $BIN -> $SIG"
openssl dgst -sha256 -verify <(openssl pkey -in "$KEY" -pubout) -signature "$SIG" "$BIN"
