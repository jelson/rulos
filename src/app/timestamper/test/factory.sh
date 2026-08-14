#!/bin/bash
#
# One-shot manufacturing provisioning: takes a virgin LectroTIC-4 PCB to
# programmed, verified, and ready to bag. No arguments.
#
# Fixture requirements: the unit under test on USB, a Black Magic Probe on its
# SWD header, the PG-4 wired straight through on all four channels, both
# instruments on the shared 10 MHz reference, and the BMP's auxiliary UART TX
# wired to the unit's Serial In.
#
# Steps: build firmware from this checkout, flash over SWD, wait for USB
# enumeration, run the factory verification suite, then factory-reset so the
# unit ships with saved defaults. Exits nonzero (with a loud FAIL) if any step
# does.

set -euo pipefail
cd "$(dirname "$0")"

banner() { echo; echo "======== $1 ========"; }

banner "build"
if [ -n "$(git status --porcelain)" ]; then
    echo "WARNING: repo has uncommitted changes; firmware will carry a -dirty stamp."
fi
(cd .. && scons)
ELF=../../../../build/timestamper/arm-stm32h523xc/timestamper.elf

banner "flash (SWD via Black Magic Probe)"
python3 ../../../util/bmpflash.py "$ELF"

banner "waiting for USB enumeration"
for i in $(seq 1 15); do
    if python3 ../util/tsctl.py idn >/dev/null 2>&1; then
        break
    fi
    if [ "$i" -eq 15 ]; then
        echo "======== FACTORY PROVISIONING FAIL: device never enumerated ========"
        exit 1
    fi
    sleep 1
done
python3 ../util/tsctl.py idn

banner "factory verification"
python3 regression_test.py --factory

banner "factory reset to shipping defaults"
python3 ../util/tsctl.py reset

banner "DONE"
echo "Unit verified and reset: $(python3 ../util/tsctl.py idn)"
echo "Ready for the bag."
