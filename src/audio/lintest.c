/************************************************************************
 *
 * This file is part of RulOS, Ravenna Ultra-Low-Altitude Operating
 * System -- the free, open-source operating system for microcontrollers.
 *
 * Written by Jon Howell (jonh@jonh.net) and Jeremy Elson (jelson@gmail.com),
 * May 2009.
 *
 * This operating system is in the public domain.  Copyright is waived.
 * All source code is completely free to use by anyone for any purpose
 * with no restrictions whatsoever.
 *
 * For more information about the project, see: www.jonh.net/rulos
 *
 ************************************************************************/

#include <fcntl.h>
#include <inttypes.h>
#include <math.h>
#include <stdio.h>
#include <sys/types.h>

uint8_t ulaw_samples[] = {
0xff, 0x7f, 0x7e, 0xfe, 0x80, 0x00,
 0x0e, 0x0f, 0x18, 0x1a, 0x24, 0x2c, 0x49, 0xcd,
 0xad, 0xa5, 0x9a, 0x98, 0x8f, 0x8e, 0x89, 0x8a, 0x0f, 0x07, 0x0e, 0x0e, 0x17, 0x19, 0x22, 0x2a,
 0x41, 0xdb, 0xaf, 0xa7, 0x9c, 0x98, 0x91, 0x8d, 0x8c, 0x84, 0x2f, 0x05, 0x0e, 0x0e, 0x15, 0x19,
 0x1f, 0x2a, 0x3a, 0xe8, 0xb5, 0xa8, 0x9e, 0x98, 0x93, 0x8d, 0x8d, 0x84, 0xbe, 0x05, 0x0d, 0x0e,
 0x13, 0x19, 0x1d, 0x28, 0x34, 0x6c, 0xba, 0xa9, 0x9f, 0x98, 0x95, 0x8d, 0x8e, 0x85, 0x9f, 0x06,
 0x0c, 0x0e, 0x11, 0x18, 0x1c, 0x27, 0x30, 0x58, 0xbf, 0xab, 0xa2, 0x99, 0x97, 0x8e, 0x8f, 0x86,
 0x94, 0x08, 0x0a, 0x0e, 0x0f, 0x18, 0x1a, 0x25, 0x2d, 0x4d, 0xc8, 0xac, 0xa4, 0x9a, 0x98, 0x8e
};

// based on sample code at:
// http://www.speech.cs.cmu.edu/comp.speech/Section2/Q2.7.html
int32_t ulaw2linear(uint8_t ulawbyte)
{
  static int32_t exp_lut[8] = {0,132,396,924,1980,4092,8316,16764};
  int8_t sign;
  uint8_t exponent;
  int32_t mantissa;
  int32_t sample;

  ulawbyte = ~ulawbyte;
  sign = (ulawbyte & 0x80);
  exponent = (ulawbyte >> 4) & 0x07;
  mantissa = ulawbyte & 0x0F;
//  printf("   byte %02x sign %2x exponent %2x mantissa %2x\n",
// 	(uint8_t) ~ulawbyte, (uint8_t) sign, (uint8_t) exponent, (uint8_t) mantissa);
  sample = exp_lut[exponent] + (mantissa << (exponent + 3));
  if (sign != 0) { sample = -sample; }

  return sample;
}


main()
{

	int i;
	for (i=0; i<sizeof(ulaw_samples); i++)
	{
		uint32_t lin_sample = ulaw2linear(ulaw_samples[i]);
		printf("%8d\n", (char) (lin_sample >> 8)+127);
	}
}

boink()
{
	int audiofd = open("/dev/dsp", O_ASYNC|O_WRONLY);
	uint8_t buf[1024];
	int i;
	for (i=0; i<sizeof(buf); i++)
	{
		double v = sin((i % 64) * 1.0 / 64 * 2 * 3.14159);
		buf[i] =  (v*127)+128;
	}
	while (1)
	{
		write(audiofd, buf, sizeof(buf));
	}
}
