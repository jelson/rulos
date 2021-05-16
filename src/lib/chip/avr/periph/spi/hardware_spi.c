/*
 * Copyright (C) 2009 Jon Howell (jonh@jonh.net) and Jeremy Elson
 * (jelson@gmail.com).
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */

#include <avr/boot.h>
#include <avr/interrupt.h>
#include <avr/io.h>
#include <util/delay_basic.h>

#include "core/hal.h"
#include "core/hardware.h"
#include "core/rulos.h"
#include "periph/spi/hal_spi.h"

//////////////////////////////////////////////////////////////////////////////
// SPI: ATMega...32P page 82, 169
// Flash: w25x16 page 17+

HALSPIHandler *g_spi_handler;

ISR(SPI_STC_vect) {
  uint8_t byte = SPDR;

  g_spi_handler->func(g_spi_handler, byte);  // read from SPDR?
}

#define DDR_SPI DDRB
#define DD_SS DDB2
#define SPI_SS GPIO_B2
#define DD_MOSI DDB3
#define DD_MISO DDB4
#define DD_SCK DDB5
// TODO these can't be right -- GPIO_B2 is allocated to BOARDSEL2
// in V11PCB.

void hal_init_spi() {
  g_spi_handler = NULL;

  // TODO I'm blasting the port B DDR here; we may care
  // about correct values for PB[0167].
  DDR_SPI = (1 << DD_MOSI) | (1 << DD_SCK) | (1 << DD_SS) | (0 << DD_MISO);
  // TODO probably should stop interrupts when configuring SPCR
  hal_spi_set_fast(FALSE);
}

void hal_spi_set_fast(bool fast) {
// defs atmega328p page 174
#define FOSC4 0
#define FOSC16 1
#define FOSC64 2
#define FOSC128 3
#define FOSC2 4
#define FOSC8 5
#define FOSC32 6
#ifdef CRYSTAL
#if CRYSTAL == 20000000
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

#define SBIT(spd, bit) (((spd) >> (bit)) & 1)

  uint8_t spcr = (1 << SPIE) | (1 << SPE) | (1 << MSTR);
  spcr |= (fast ? ((SBIT(FAST, 1) << SPR1) | (SBIT(FAST, 0) << SPR0))
                : ((SBIT(SLOW, 1) << SPR1) | (SBIT(SLOW, 0) << SPR0)));
  SPCR = spcr;
  SPSR = (fast ? (SBIT(FAST, 2) << SPI2X) : (SBIT(SLOW, 2) << SPI2X));
}

void hal_spi_select_slave(bool select) { gpio_set_or_clr(SPI_SS, !select); }

void hal_spi_set_handler(HALSPIHandler *handler) { g_spi_handler = handler; }

void hal_spi_send(uint8_t byte) { SPDR = byte; }
