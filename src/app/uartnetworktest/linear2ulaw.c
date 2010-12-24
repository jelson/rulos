#include <stdint.h>

// http://www.dsprelated.com/groups/speechcoding/show/444.php
// John C. Bellamy's Digital Telephony, 1982,
#define CLIP 8159
#define BIAS (0x84)

inline static short search(short val, short *table, short size)
{
	short i;

	for (i = 0; i < size; i++)
	{
		if (val <= *table++)
		{
			return (i);
		}
	}
	return (size);
}

static int16_t seg_end[8] = {
	0x3F, 0x7F, 0xFF, 0x1FF, 0x3FF, 0x7FF, 0xFFF, 0x1FFF };

uint8_t linear2ulaw(int16_t pcm_val)
{
	short mask;
	short seg;
	unsigned char uval;

	/* Get the sign and the magnitude of the value. */
	pcm_val = pcm_val >> 2;
	if (pcm_val < 0) {
		pcm_val = -pcm_val;
		mask = 0x7f;
	} else {
		mask = 0xff;
	}
	if (pcm_val > CLIP) pcm_val = CLIP; /* clip the magnitude */
	pcm_val += (BIAS >> 2);

	/* Convert the scaled magnitude to segment number. */
	seg = search(pcm_val, seg_end, 8);

	/*
	* Combine the sign, segment, quantization bits;
	* and complement the code word.
	*/
	if (seg >= 8) /* out of range, return maximum value. */
	{
		return (uint8_t) (0x7F ^ mask);
	}
	else
	{
		uval = (uint8_t) (seg << 4) | ((pcm_val >> (seg + 1)) & 0xF);
		return (uval ^ mask);
	}

}

void linear2ulaw_buf(int16_t *pcm_buf, uint8_t *ulaw_buf, int samples)
{
	int i;
	for (i=0; i<samples; i++)
	{
		ulaw_buf[i] = linear2ulaw(pcm_buf[i]);
	}
}
