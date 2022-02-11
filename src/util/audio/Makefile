all: sdcard.img audio_sim2

sdcard: compile_audio.py ../../lib/periph/audio/sound.def ../../../ext/Media/audio/fx/* ../../../ext/Media/audio/disco/*
	./compile_audio.py -fatfs ../../lib/periph/audio/sound.def ../../../ext/Media/audio sdcard

sdcard.img: sdcard ./make-sdcard-img.py
	sudo ./make-sdcard-img.py

playsample: sdcard
	play --channels 2 --bits 16 -r 50000 -e signed-integer tmpdir/sfx/00004.raw 

audio_sim2: audio_sim2.c
	gcc -g audio_sim2.c -o audio_sim2

# Not sure what this stuff did:
#all: audio_sim lintest
#
#audio_sim: audio_sim.c
#	gcc -g audio_sim.c -o audio_sim
#
#lintest: lintest.c
#	gcc -g lintest.c -o lintest -lm
#
