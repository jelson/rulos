#!/bin/bash
#
# Flash duktig-v2 onto the target over the Black Magic Probe.
# Thin wrapper around the shared src/util/bmpflash.py (BMP autodetect,
# load+verify, reset). Extra args pass through, e.g.:
#   ./program.sh             # flash + verify + reset
#   ./program.sh --erase     # mass-erase first (unbrick a torn-flash unit)
#   ./program.sh --reset-only
set -e

HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
BMPFLASH="$HERE/../../util/bmpflash.py"

#PLATFORM=arm-stm32g030x6
PLATFORM=arm-stm32c011x6
BIN="$HERE/../../../build/duktig-v2/$PLATFORM/duktig-v2.elf"

exec "$BMPFLASH" "$BIN" "$@"
