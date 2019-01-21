#pragma once

uint8_t linear2ulaw(int16_t pcm_val);
void linear2ulaw_buf(int16_t *pcm_buf, uint8_t *ulaw_buf, int samples);
