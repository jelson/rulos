#ifndef _BSS_CANARY_H
#define _BSS_CANARY_H

#include <rocket.h>

void bss_canary_init(uint8_t *bss_end);

#endif // _BSS_CANARY_H
