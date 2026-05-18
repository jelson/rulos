# dfutest — RULOS USB-DFU facility test

Minimal USB-CDC app that exists only to validate the shared RULOS DFU
firmware-update facility end to end on an STM32H523.

## Adding DFU to a real app

Nothing to write. **Any app that uses the `usb-cdc-stm32` peripheral
has DFU automatically:** the core facility
(`chip/arm/stm32/core/dfu`) and the DFU runtime interface (composite
CDC+DFU descriptor) are built in. Requirements:

- Give the app a stable USB identity in its SConstruct
  (`-DUSBD_VID=0x....`, `-DUSBD_PID=0x....`) so the updater can find it.
- STM32H5 only, and the part must stay `PRODUCT_STATE=Open`, TZEN=0,
  RDP 0 (the default). DFU jumps into the ST ROM bootloader; outside
  that state the jump faults (recoverable only over SWD).

## Instruction to give an end user

One command (replace `VID:PID` with the app's runtime USB id; the ROM
bootloader is always ST `0483:df11`):

```
sudo dfu-util -d VID:PID,0483:df11 -a 0 -s 0x08000000:leave -D firmware.bin
```

dfu-util finds the running device, sends DFU_DETACH (it reboots into
the ROM bootloader), waits for re-enumeration, writes the image, and
`:leave` runs it. A failed/aborted update never bricks the unit —
re-run the same command.

Only prerequisite: install `dfu-util`. Run it with `sudo` — it needs
raw USB access.

## Running this test

Needs the H523 board on a Black Magic Probe (SWD) plus its USB cable.

```
./dfu_test.py
```

Builds Rev A and Rev B as two independent images
(`scons --define=DFUTEST_FW_VERSION=... --build-dir=...`, no source
edits), flashes Rev A over SWD as the trusted baseline, then performs
the single real `dfu-util` update to Rev B and verifies the banner
changed. SWD is also the recovery path.
