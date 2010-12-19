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
	r_bool cmd_acknowledged;
	r_bool reply_started;

	// experiment: do most work out of interrupt handler
	uint8_t data;
	SPIAct spiact;
} SPI;

void spi_init(SPI *spi);
void spi_start(SPI *spi, SPICmd *spic);

//////////////////////////////////////////////////////////////////////////////

typedef struct {
	Activation act;
	uint16_t blocksize;
	SPI spi;
	SPICmd spic;
	r_bool complete;
	BunchaClocks bunchaClocks;
	Activation *done_act;
	uint8_t init_state;
	uint8_t read_cmdseq[6];
	r_bool busy;
} SDCard;

void sdc_init(SDCard *sdc);
void sdc_initialize(SDCard *sdc, Activation *done_act);
void sdc_read(SDCard *sdc, uint32_t offset, uint8_t *buf, uint16_t buflen, Activation *done_act);

#endif // _SDCARD_H
