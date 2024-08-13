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

//Outputs
#define GREEN_LED  GPIO_B1
#define ORANGE_LED GPIO_B2
#define RANGE_SEL  GPIO_B0
#define CHRG_SHDWN GPIO_B9
#define CLK_DIR    GPIO_A15
#define DIO_DIR    GPIO_C6
#define U5TX_DIR   GPIO_B5
#define U5RX_DIR   GPIO_B8

//Inputs
#define BOOT_PIN   GPIO_A14
#define LOG_PIN    GPIO_A0

// power measurement
#define BAT_POWERMEASURE_ADDR   0b1000001
#define MODEM_POWERMEASURE_ADDR 0b1000000

UartState_t console;
flash_dumper_t flash_dumper;
serial_reader_t psoc_console_tx;

currmeas_state_t bat_cms, modem_cms;

void ClearOptionByte (void){

  /* Clear the LOCK bit in FLASH->CR (precondition for option byte flash) */
  FLASH->KEYR = 0x45670123;
  FLASH->KEYR = 0xCDEF89AB;
  /* Clear the OPTLOCK bit in FLASH->CR */
  FLASH->OPTKEYR = 0x08192A3B;
  FLASH->OPTKEYR = 0x4C5D6E7F;

    /* Enable legacy mode (BOOT0 bit defined by BOOT0 pin) */
  /* by clearing the nBOOT_SELection bit */
  FLASH->OPTR &= ~FLASH_OPTR_nBOOT_SEL;
  
  /* check if there is any flash operation */
  while( (FLASH->SR & FLASH_SR_BSY1) != 0 )
    ;
  
  /* start the option byte flash */
  FLASH->CR |= FLASH_CR_OPTSTRT;
  /* wait until flashing is done */
  while( (FLASH->SR & FLASH_SR_BSY1) != 0 )
    ;  

  /* do a busy delay, for about one second, check BSY1 flag to avoid compiler loop optimization */
  for( unsigned long i = 0; i < 2000000; i++ )
    if ( (FLASH->SR & FLASH_SR_BSY1) != 0 )
      break;
  
  /* load the new value and do a system reset */
  /* this will behave like a goto to the begin of this main procedure */
  FLASH->CR |= FLASH_CR_OBL_LAUNCH;

}

void JumpToBootloader (void)
{
  void (*SysMemBootJump)(void);
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
  NVIC->ICPR[4]=0xFFFFFFFF;      

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
  gpio_make_input_enable_pullup(LOG_PIN);

  gpio_make_output(ORANGE_LED);
  gpio_make_output(GREEN_LED);

  gpio_make_output(RANGE_SEL);
  gpio_set(RANGE_SEL);
  gpio_make_output(CHRG_SHDWN);
  gpio_set(CHRG_SHDWN);

  //set all translators to input
  gpio_make_output(CLK_DIR);
  gpio_clr(CLK_DIR);
  gpio_make_output(DIO_DIR);
  gpio_clr(DIO_DIR);
  gpio_make_output(U5TX_DIR);
  gpio_clr(U5TX_DIR);
  gpio_make_output(U5RX_DIR);
  gpio_clr(U5RX_DIR);  

  //check the flash option.  If it is not clear, fallback to manually reading the pin and jumping.
  if ( (FLASH->OPTR & FLASH_OPTR_nBOOT_SEL) != 0 ){
    ClearOptionByte();
    if(gpio_is_set(BOOT_PIN))
      JumpToBootloader();
  }

  init_clock(10000, TIMER1);


  // initialize console USB Uart
  #if 1
  // initialize console uart
  uart_init(&console, CONSOLE_UART_NUM, 1000000);
  log_bind_uart(&console);
  LOG("Lora Listener starting, rev " STRINGIFY(GIT_COMMIT));
  #else
  // initialize console uart
  uart_init(&console, CONSOLE_UART_NUM, 1000000);
  log_bind_uart(&console);
  LOG("Lora Listener starting, rev " STRINGIFY(GIT_COMMIT));
  #endif

  // initialize flash dumper
  flash_dumper_init(&flash_dumper);

  // initialize serial readers
  serial_reader_init(&psoc_console_tx, PSOC_TX_UART_NUM, 1000000, &flash_dumper,
                     NULL, NULL);

  // initialize current measurement. docs say calibration register should be
  // trunc[0.04096 / (current_lsb * R_shunt)]

  //check logger button, set sleep measurement if held at startup.
  int i = 5;
  if(gpio_is_clr(LOG_PIN)) {
    
    while (i>0){    
    gpio_set(ORANGE_LED);
    delay_us(250000);
    gpio_clr(ORANGE_LED);
    delay_us(250000);
    i--;
    }

    gpio_clr(RANGE_SEL);
    // BAT: R_shunt is 100R, Iresolution = 0.1
    // .04096 / (0.1 * 100 ohms) = .004096 * 10K
    currmeas_init(&bat_cms, BAT_POWERMEASURE_ADDR, VOLT_PRESCALE_DIV1, 45, //calibrated with sourcemeter.
                  10,
                  1, &flash_dumper);
  } else {

    while (i>0){    
    gpio_set(GREEN_LED);
    delay_us(250000);
    gpio_clr(GREEN_LED);
    delay_us(250000);
    i--;
    }

    // BAT: R_shunt is 150 milliohms, Iresolution = 66.7
    // .04096 / (66.7 uA * 0.150 ohms) = 4.09395 x 10K 
    currmeas_init(&bat_cms, BAT_POWERMEASURE_ADDR, VOLT_PRESCALE_DIV1, 40940, 
                  4, //seems best approximation
                  1, &flash_dumper);
  }

  // LORA: Rshunt = 2 ohms, Iresolution = 5uA
  // .04096 / (.005 A * 2 ohms) = 4.096 x 10K
  currmeas_init(&modem_cms, MODEM_POWERMEASURE_ADDR, VOLT_PRESCALE_DIV1, 40960,
                5,  
                2, &flash_dumper);                     

  // enable periodic blink to indicate liveness
  schedule_now(indicate_alive, NULL);

  scheduler_run();
  return 0;
}
