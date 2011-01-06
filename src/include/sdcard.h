#ifndef _SDCARD_H
#define _SDCARD_H

#include "rocket.h"
#include "hardware_audio.h"

//////////////////////////////////////////////////////////////////////////////

typedef struct
{
	HALSPIHandler handler;
	int numclocks_bytes;
	Activation *done_act;
} BunchaClocks;

//////////////////////////////////////////////////////////////////////////////

typedef struct
{
	uint8_t *cmd;
	uint8_t cmdlen;
	uint8_t cmd_expect_code;
	uint8_t *replydata;
	uint16_t replylen;
	Activation *done_act;
	r_bool complete;
} SPICmd;

typedef struct {
	Activation act;
	struct s_SPI *spi;
} SPIAct;
typedef struct s_SPI
{
	HALSPIHandler handler;
	SPICmd *spic;
	uint8_t cmd_i;
	uint16_t cmd_wait;
	uint16_t reply_wait;
	uint16_t reply_i;

	// experiment: do most work out of interrupt handler
	uint8_t data;
	SPIAct spiact;
} SPI;

void spi_init(SPI *spi);
void spi_start(SPI *spi, SPICmd *spic);

//////////////////////////////////////////////////////////////////////////////

// Memory is too scarce to have separate callers with their own buffers;
// so we allocate a single block buffer, and share it with all callers.
#define SDBUFSIZE	512
#define SDCRCSIZE	2

typedef struct {
	Activation act;
	uint16_t blocksize;
	SPI spi;
	SPICmd spic;
	r_bool complete;
		// TODO One ugly problem: this shared 'complete' variable
		// is an thread-unsafe 'errno' -- if someone finds us !busy,
		// and calls us before the first caller sees his complete,
		// the first caller might incorrectly infer failure. Bummer.
	BunchaClocks bunchaClocks;
	Activation *done_act;
	uint8_t read_cmdseq[6];
	r_bool busy;
	uint8_t blockbuffer[SDBUFSIZE+SDCRCSIZE];
} SDCard;

void sdc_init(SDCard *sdc);
void sdc_initialize(SDCard *sdc, Activation *done_act);
r_bool sdc_read(SDCard *sdc, uint32_t offset, Activation *done_act);
	// FALSE if sdc was busy.

#endif // _SDCARD_H
