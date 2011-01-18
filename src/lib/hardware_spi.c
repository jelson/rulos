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


#include <avr/boot.h>
#include <avr/io.h>
#include <avr/interrupt.h>
#include <util/delay_basic.h>

#include "rocket.h"
#include "hardware.h"
#include "board_defs.h"
#include "hal.h"



//////////////////////////////////////////////////////////////////////////////
// SPI: ATMega...32P page 82, 169
// Flash: w25x16 page 17+

HALSPIHandler *g_spi_handler;

ISR(SPI_STC_vect)
{
	uint8_t byte = SPDR;

	g_spi_handler->func(g_spi_handler, byte);	// read from SPDR?
}

#define DDR_SPI	DDRB
#define DD_SS	DDB2
#define SPI_SS	GPIO_B2
#define DD_MOSI	DDB3
#define DD_MISO	DDB4
#define DD_SCK	DDB5
// TODO these can't be right -- GPIO_B2 is allocated to BOARDSEL2
// in V11PCB.

void hal_init_spi()
{
	g_spi_handler = NULL;

	// TODO I'm blasting the port B DDR here; we may care
	// about correct values for PB[0167].
	DDR_SPI = (1<<DD_MOSI)|(1<<DD_SCK)|(1<<DD_SS)|(0<<DD_MISO);
	// TODO probably should stop interrupts when configuring SPCR
	hal_spi_set_fast(FALSE);
}

void hal_spi_set_fast(r_bool fast)
{
// defs atmega328p page 174
#define FOSC4	0
#define FOSC16	1
#define FOSC64	2
#define FOSC128	3
#define FOSC2	4
#define FOSC8	5
#define FOSC32	6
#ifdef CRYSTAL
	#if CRYSTAL==20000000
		#define SLOW FOSC128
		#define FAST FOSC16
	#else
		#error unimplemented crystal speed
	#endif
#else
	// assume 8MHz internal RC
	#define SLOW FOSC64
	#define FAST FOSC8
#endif

#define SBIT(spd,bit)	(((spd)>>(bit))&1)

	uint8_t spcr = (1<<SPIE) | (1<<SPE) | (1<<MSTR);
	spcr |= (fast ?
			  ((SBIT(FAST,1)<<SPR1) | (SBIT(FAST,0)<<SPR0))
			: ((SBIT(SLOW,1)<<SPR1) | (SBIT(SLOW,0)<<SPR0)));
	SPCR = spcr;
	SPSR = (fast ? (SBIT(FAST,2)<<SPI2X) : (SBIT(SLOW,2)<<SPI2X));
}

void hal_spi_select_slave(r_bool select)
{
	gpio_set_or_clr(SPI_SS, !select);
}

void hal_spi_set_handler(HALSPIHandler *handler)
{
	g_spi_handler = handler;
}

void hal_spi_send(uint8_t byte)
{
	SPDR = byte;
}


