#pragma once

#include "animate.h"

typedef struct s_frobutton {
	Animate* animate;
} Frobutton;

void frobutton_init(Frobutton* fb, Animate* an);
