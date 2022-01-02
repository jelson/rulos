== Getting GDB to Work ==

Docs:

https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-guides/jtag-debugging/index.html

1)
Buy an ESP-PROG:
https://www.digikey.com/en/products/detail/espressif-systems/ESP-PROG/10259352

"Program" port is the same as what's built in to most target boards such as the
NodeMCU: an FTDI USB serial port that uses a couple of the serial DTR/DSR lines
for out-of-band signalling. In other words, the ESP32's "Program" port, similar
to a NodeMCU USB port, can program the chip by resetting it with IO0 set to
indicate it should boot into a bootloader ready to receive a program via the
serial port.

The "JTAG" port is the full JTAG implementation that can be used in conjunction
with OpenOCD to debug with GDB.


2) Confirm JTAG port power selector set to 3v3

3) Attach 5 wires from ESP32's JTAG port to target board:
     GND, TMS, TCK, TDO, TDI

     ESP-PROG pinout found on page 4 of https://media.digikey.com/pdf/Data%20Sheets/Espressif%20PDFs/Intro_to_the_ESP-Prog_Brd_Web.pdf

4) Install OpenOCD:

   a) git clone https://github.com/espressif/openocd-esp32

   b) ./bootstrap

   c) ./configure CC=gcc-10
   (Note: gcc 11 has some new warnings that cause compilation to fail, so we
   used GCC 10)

   d) make

5) Install libpython2.7, required by the xtensa cross-gdb:

sudo apt-get install libpython2.7

6) Run openocd:

./src/openocd  -s tcl -f board/esp32-wrover-kit-3.3v.cfg

7) In another window while openocd is running, switch to the application's source directory and run:

../../../../build/compilers/esp32/1.0.6/xtensa-esp32-elf-gcc/xtensa-esp32-elf/bin/xtensa-esp32-elf-gdb ../../../../build/esp32test/esp32/esp32test.elf -ex 'target remote localhost:3333'
