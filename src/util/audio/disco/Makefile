OGGS=$(wildcard *.ogg)
AUS=$(patsubst %.ogg,%.au,$(OGGS))

all: $(AUS) disco_index.h

disco_index.h: Makefile
	echo $(AUS) | tr ' ' '\n' | sed 's/^/\t/' | sed 's/.au$$/,/' > $@

%.au: %.ogg
	sox $^ --channels 1 --bits 8 --rate 8000 $@ vol 0.75

%.a2.au: %.ogg
	sox $^ --channels 1 --bits 8 --rate 12000 $@ vol 0.75
