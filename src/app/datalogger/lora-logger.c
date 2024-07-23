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

#include "core/hardware.h"
#include "core/rulos.h"
#include "flash_dumper.h"
#include "periph/uart/uart.h"
#include "serial_reader.h"
#include "curr_meas.h"
#include "ina219.h"

// uart definitions
#define CONSOLE_UART_NUM 0
#define PSOC_TX_UART_NUM 1

#define GREEN_LED  GPIO_B1
#define ORANGE_LED GPIO_B2
#define BOOT_PIN   GPIO_A14

// power measurement
#define GPS_POWERMEASURE_ADDR   0b1000000
#define MODEM_POWERMEASURE_ADDR 0b1000001

UartState_t console;
flash_dumper_t flash_dumper;
serial_reader_t psoc_console_tx;

currmeas_state_t gps_cms, modem_cms;

void JumpToBootloader (void)
{
void (*SysMemBootJump)(void);


/* Set a vector addressed with STM32 Microcontrollers names */
/* Each vector position contains an address to the boot loader entry point */

	volatile uint32_t BootAddr;

	BootAddr = 0x1FFF0000;

	/* Disable all interrupts */
	__disable_irq();

	/* Disable Systick timer */
	SysTick->CTRL = 0;

	/* Set the clock to the default state */
	HAL_RCC_DeInit();

	/* Clear Interrupt Enable Register & Interrupt Pending Register */

  NVIC->ICER[0]=0xFFFFFFFF;
  NVIC->ICPR[0]=0xFFFFFFFF;
  NVIC->ICER[1]=0xFFFFFFFF;
  NVIC->ICPR[1]=0xFFFFFFFF;
  NVIC->ICER[2]=0xFFFFFFFF;
  NVIC->ICPR[2]=0xFFFFFFFF;
  NVIC->ICER[3]=0xFFFFFFFF;
  NVIC->ICPR[3]=0xFFFFFFFF;
  NVIC->ICER[4]=0xFFFFFFFF;
  NVIC->ICPR[5]=0xFFFFFFFF;      

	/* Re-enable all interrupts */
	__enable_irq();

	/* Set up the jump to boot loader address + 4 */
	SysMemBootJump = (void (*)(void)) (*((uint32_t *) (BootAddr + 4)));

	/* Set the main stack pointer to the boot loader stack */
	__set_MSP(*(uint32_t *)BootAddr);

	/* Call the function to jump to boot loader location */
	SysMemBootJump();

	/* Jump is done successfully */
	while (1)
	{
		/* Code should never reach this loop */
	}
}

static void turn_off_led(void *data) {
  gpio_clr(GREEN_LED);
  gpio_clr(ORANGE_LED);
}

static void indicate_alive(void *data) {
  schedule_us(5000000, indicate_alive, NULL);
  if (flash_dumper.ok && serial_reader_is_active(&psoc_console_tx)) {
    gpio_set(GREEN_LED);
  } else {
    gpio_set(ORANGE_LED);
  }
  schedule_us(30000, turn_off_led, NULL);
}

int main() {
  rulos_hal_init();

  gpio_make_input_enable_pullup(BOOT_PIN);

  if(gpio_is_clr(BOOT_PIN))
    JumpToBootloader();
  

  init_clock(10000, TIMER1);

  // initialize console uart
  uart_init(&console, CONSOLE_UART_NUM, 1000000);
  log_bind_uart(&console);
  LOG("Lora Listener starting, rev " STRINGIFY(GIT_COMMIT));

  // initialize flash dumper
  flash_dumper_init(&flash_dumper);

  // initialize serial readers
  serial_reader_init(&psoc_console_tx, PSOC_TX_UART_NUM, 1000000, &flash_dumper,
                     NULL, NULL);

  // initialize current measurement. docs say calibration register should be
  // trunc[0.04096 / (current_lsb * R_shunt)]

  // GPS: R_shunt is 150 milliohms, Iresolution = 66.7
  // .04096 / (66.7 uA * 0.150 ohms) = 4094
  currmeas_init(&gps_cms, GPS_POWERMEASURE_ADDR, VOLT_PRESCALE_DIV1, 54613, 5,
                1, &flash_dumper);

  // LTE: Rshunt = 0.50 ohms, Iresolution = 10uA
  // .04096 / (10 uA * 0.5 ohms) = 8192
  currmeas_init(&modem_cms, MODEM_POWERMEASURE_ADDR, VOLT_PRESCALE_DIV1, 8192,
                10,  // we read in 10 microamps
                2, &flash_dumper);                     

  // enable periodic blink to indicate liveness
  schedule_now(indicate_alive, NULL);

  gpio_make_output(ORANGE_LED);
  gpio_make_output(GREEN_LED);

  scheduler_run();
  return 0;
}
