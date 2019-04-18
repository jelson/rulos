#!/usr/bin/python3

# Generate a file with 2^20 (~1 million) binary 32-bit numbers. The
# intent is to copy the generated file on to an SD card and read it
# from FatFs to ensure that sectors are being returned correctly and
# in the right order.
#
# We don't want to just write down the first million numbers; that
# won't test the high-order byte. We also don't want to increment by 4096
# because that won't test the low order byte.
#
# Instead, I've chosen the increment to be 4093, which is the largest
# prime number less than 4096. This ensures all bytes are tested and
# is a sequence that's easy for the reader on the microcontroller to
# replicate and validate.
#
# First ten 4-byte groups:
# 0000 0000
# 0000 0ffd
# 0000 1ffa
# 0000 2ff7
# 0000 3ff4
# 0000 4ff1
# 0000 5fee
# 0000 6feb
# 0000 7fe8
# 0000 8fe5
#
# Last ten 4-byte groups:
# ffcf 601e
# ffcf 701b
# ffcf 8018
# ffcf 9015
# ffcf a012
# ffcf b00f
# ffcf c00c
# ffcf d009
# ffcf e006
# ffcf f003
#

f = open("1meg.bin", "wb")

for i in range(1 << 20):
    num = i * 4093;
    f.write(bytes(num.to_bytes(4, byteorder='big')))
    

