sdcard:
	./compile_audio.py -fatfs ../../lib/periph/audio/sound.def ../../../Media/audio sdcard

playsample: sdcard
	play --channels 2 --bits 16 -r 50000 -e signed-integer tmpdir/sfx/00004.raw 

all: audio_sim lintest

audio_sim: audio_sim.c
	gcc -g audio_sim.c -o audio_sim

lintest: lintest.c
	gcc -g lintest.c -o lintest -lm

