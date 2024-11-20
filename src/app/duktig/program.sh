BIN=~/projects/rulos/build/duktig-v2/arm-stm32g030x6/duktig-v2.elf
#BIN=~/projects/rulos/build/duktig-v2/arm-stm32c011x6/duktig-v2.elf

gdb-multiarch \
    $BIN \
    -ex "set confirm off" \
    -ex "set pagination off" \
    -ex "tar ext /dev/ttyACM0" \
    -ex "mon tpwr ena" \
    -ex "mon conn enable" \
    -ex "mon swd" \
    -ex "at 1" \
    -ex "load" \
    -ex "mon hard" \
    -ex "quit"
