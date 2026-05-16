#!/bin/bash
#
# Build the LectroTIC-4 firmware and flash it to the DUT over the
# attached Black Magic Probe, verifying the write and resetting into
# the new image.
#
# The BMP GDB port is found by USB enumeration (its stable
# /dev/serial/by-id name), not a fixed ttyACMx -- ttyACM numbering
# depends on plug order and what else (incl. the LectroTIC-4 itself)
# is connected.
#
# Usage:
#   flash.sh                    build, then flash the default elf
#   flash.sh --no-build         flash the existing elf as-is
#   flash.sh --port /dev/ttyACM3   override BMP autodetection
#   flash.sh path/to/foo.elf    flash a specific elf
#
set -euo pipefail

REPO=$(git -C "$(dirname "$0")" rev-parse --show-toplevel)
APPDIR="$REPO/src/app/timestamper"
ELF="$REPO/build/timestamper/arm-stm32h523xc/timestamper.elf"
PORT=
BUILD=1

# The Black Magic Probe enumerates two CDC-ACM interfaces: -if00 is
# the GDB server (what we drive), -if02 is its UART passthrough.
detect_bmp() {
  local m=( /dev/serial/by-id/usb-Black_Magic*-if00 )
  if [ ! -e "${m[0]}" ]; then
    echo "flash.sh: no Black Magic Probe found (looked for" \
         "/dev/serial/by-id/usb-Black_Magic*-if00). Plugged in?" \
         "Use --port to override." >&2
    exit 1
  fi
  if [ "${#m[@]}" -gt 1 ]; then
    echo "flash.sh: multiple Black Magic Probes:" >&2
    printf '  %s\n' "${m[@]}" >&2
    echo "Use --port to pick one." >&2
    exit 1
  fi
  readlink -f "${m[0]}"
}

while [ $# -gt 0 ]; do
  case "$1" in
    --no-build) BUILD=0 ;;
    --port) PORT="$2"; shift ;;
    *.elf) ELF="$1" ;;
    *) echo "flash.sh: unknown argument: $1" >&2; exit 2 ;;
  esac
  shift
done

PORT="${PORT:-$(detect_bmp)}"
echo "flash.sh: BMP GDB port: $PORT" >&2

if [ "$BUILD" = 1 ]; then
  ( cd "$APPDIR" && scons )
fi

exec gdb-multiarch "$ELF" \
  -ex "set confirm off" \
  -ex "set pagination off" \
  -ex "tar ext $PORT" \
  -ex "mon conn enable" \
  -ex "mon swd" \
  -ex "at 1" \
  -ex "load" \
  -ex "compare-sections" \
  -ex "mon reset" \
  -ex "kill" \
  -ex "quit"
