#ifndef _POTSTICKER_H
#define _POTSTICKER_H

#include "rocket.h"
#include "input_controller.h"

typedef struct s_potsticker {
	ActivationFunc func;
	uint8_t adc_channel;
	InputInjectorIfc *ifi;
	uint8_t detents;
	int8_t hysteresis;
	char fwd;
	char back;

	int8_t last_digital_value;
} PotSticker;

void init_potsticker(PotSticker *ps, uint8_t adc_channel, InputInjectorIfc *ifi, uint8_t detents, char fwd, char back);

#endif // _POTSTICKER_H
