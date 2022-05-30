/usr/local/bin/gcc-arm-none-eabi-10-2020-q4-major/bin/arm-none-eabi-gdb \
    ~/projects/rulos/build/duktig-v2/arm-stm32g030x6/duktig-v2.elf \
    -ex "set confirm off" \
    -ex "tar ext /dev/ttyACM0" \
    -ex "mon swd" \
    -ex "at 1" \
    -ex "load" \
    -ex "mon hard" \
    -ex "quit"

