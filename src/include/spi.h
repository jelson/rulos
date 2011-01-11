#ifndef _SPI_H
#define _SPI_H

#include "rocket.h"
#include "hardware_audio.h"
#include "seqmacro.h"

typedef struct
{
	uint8_t *cmd;
	uint8_t cmdlen;
	uint8_t cmd_expect_code;
	uint8_t *reply_buffer;
	uint16_t reply_buflen;
	Activation *done_act;
	r_bool error;
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

	// do most work out of interrupt handler (in an activation)
	uint8_t data;
	SPIAct spiact;
} SPI;

void spi_init(SPI *spi);
void spi_start(SPI *spi, SPICmd *spic);
void spi_resume_transfer(SPI *spi, uint8_t *reply_buffer, uint16_t reply_buflen);
void spi_finish(SPI *spi);

#endif // _SPI_H
