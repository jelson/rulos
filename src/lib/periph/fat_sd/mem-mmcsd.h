/*
IBEX UK LTD http://www.ibexuk.com
Electronic Product Design Specialists
RELEASED SOFTWARE

The MIT License (MIT)

Copyright (c) 2013, IBEX UK Ltd, http://ibexuk.com

Permission is hereby granted, free of charge, to any person obtaining a copy of
this software and associated documentation files (the "Software"), to deal in
the Software without restriction, including without limitation the rights to
use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of
the Software, and to permit persons to whom the Software is furnished to do so,
subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS
FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR
COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER
IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
*/
// Project Name:		FAT FILING SYSTEM FAT16 & FAT 32 DRIVER
// MMC / SD CARD DRIVER C CODE HEADER FILE

//-----------------------------------
//----- SPI PORT SETUP REQUIRED -----
//-----------------------------------
// 8 bit, MSB transmitted first
// Clock low in idle bus state
// Input data is valid on the rising edge of the clock (Our transmit transistion
// occurs on transition from active (high) to Idle (low) clock state)

//*****************************
//*****************************
//********** DEFINES **********
//*****************************
//*****************************
#ifndef MMCSD_C_INIT  // Do only once the first time this file is used
#define MMCSD_C_INIT

//----------------------------------------------
//----- DEFINE TARGET COMPILER & PROCESSOR -----				//<<<<<
//CHECK FOR A NEW APPLICATION <<<<<
//----------------------------------------------
//(ONLY 1 SHOULD BE INCLUDED, COMMENT OUT OTHERS - ALSO SET IN THE OTHER DRIVER
//.h FILE) #define	MMCSD_USING_MICROCHIP_C18_COMPILER #define
//MMCSD_USING_MICROCHIP_C30_COMPILER #define
//MMCSD_USING_MICROCHIP_C32_COMPILER #define
//MMCSD_USING_CROSSWORKS_ARM_COMPILER
//<< add other compiler types here

//--------------------------------
//----- GENERAL USER OPTIONS -----
//--------------------------------
#define FFS_ACCEPT_SDHC_CARDS  // Comment out to not accept SDHC cards

//----------------------
//----- IO DEFINES -----
////<<<<< CHECK FOR A NEW APPLICATION <<<<<
//----------------------

#ifdef MMCSD_USING_MICROCHIP_C18_COMPILER
//########################################
//##### USING MICROCHIP C18 COMPILER #####
//########################################

//----- CONTROL PINS -----
#define FFS_CE(state) \
  LATCbits.LATC1 = state  // Chip select pin (set high on chip power-up)
#define FFC_DI \
  PORTCbits.RC4  // DO pin of SD card, DI pin of processor (used to check if pin
                 // is being pulled low by card)

//----- SPI BUS CONTROL REGISTERS -----
//#define	FFS_SPI_BUF_FULL			SSP1STATbits.BF
////>0 when the SPI receive buffer contains a received byte, also signifying
//that transmit is complete
//#define	FFS_SPI_TX_BYTE(data)		SSP1BUF = data
////Macro to write a byte and start transmission over the SPI bus #define
//FFS_SPI_RX_BYTE_BUFFER		SSP1BUF
////Register to read last received byte from
//##### Due to silicon bugs in the PIC18F4620 the following alternative defines
//are needed as workarounds #####
#define FFS_SPI_BUF_FULL PIR1bits.SSPIF
#define FFS_SPI_TX_BYTE(data) \
  PIR1bits.SSPIF = 0;         \
  SSPBUF = data
#define FFS_SPI_RX_BYTE_BUFFER SSPBUF

//----- SPI BUS SPEED -----
// SPI BUS LOW SPEED (Min 100KHz, Max 400KHz)  (Min speed required for ACDM41
// initalisation)
//- Adjust as required for the microcontroller and instruction clock frequency
//being used
#define SPI_BUS_SET_TO_LOW_SPEED \
  SSPCON1bits.SSPM3 = 0;         \
  SSPCON1bits.SSPM2 = 0;         \
  SSPCON1bits.SSPM1 = 1;         \
  SSPCON1bits.SSPM0 = 0;

// SPI BUS FULL SPEED FOR MMC CARD (Max 20MHz)
//- Adjust as required for the microcontroller and instruction clock frequency
//being used
#define SPI_BUS_SET_TO_FULL_SPEED_MMC \
  SSPCON1bits.SSPM3 = 0;              \
  SSPCON1bits.SSPM2 = 0;              \
  SSPCON1bits.SSPM1 = 0;              \
  SSPCON1bits.SSPM0 = 0;

// SPI BUS FULL SPEED FOR SD CARD (Max 25MHz)
//- Adjust as required for the microcontroller and instruction clock frequency
//being used
#define SPI_BUS_SET_TO_FULL_SPEED_SD \
  SSPCON1bits.SSPM3 = 0;             \
  SSPCON1bits.SSPM2 = 0;             \
  SSPCON1bits.SSPM1 = 0;             \
  SSPCON1bits.SSPM0 = 0;

//----- CARD DETECT PIN -----
//(Defined like this so the pin can be connected to an input latch IC instead of
//directly to the processor if desired)
#define FFS_CD_PIN_REGISTER PORTC
#define FFS_CD_PIN_BIT \
  0x01  //(0x80, 0x40, 0x20, 0x10, 0x08, 0x04, 0x02 or 0x01)
//#define	FFS_CD_PIN_FUNCTION
////Optional function to call to get the FFS_RESET_PIN_REGISTER.  Comment out if
//not requried
//#define	FFS_CD_PIN_NC
////Include if card detect pin is normally closed (breaks when card inserted) or
//comment out if normally open (0V common assumed)

//----- WRITE PROTECT PIN -----
//(Defined like this so the pin can be connected to an input latch IC instead of
//directly to the processor if desired)
#define FFS_WP_PIN_REGISTER \
  PORTC  // To disable write protect pin checking comment out this define
#define FFS_WP_PIN_BIT \
  0x04  //(0x80, 0x40, 0x20, 0x10, 0x08, 0x04, 0x02 or 0x01)
//#define	FFS_WP_PIN_FUNCTION
////Optional function to call to get the FFS_RESET_PIN_REGISTER.  Comment out if
//not requried
#define FFS_WP_PIN_NC  // Include if card detect pin is normally closed (breaks
                       // when write protect is active) or comment out if
                       // normally open (closes when write protect is active)(0V
                       // common assumed)

//----- MISC -----
#define FFS_DRIVER_GEN_512_BYTE_BUFFER \
  ffs_general_buffer  // This general buffer is used by driver functions

#define DO_BUSY_STATE_ACCESS_DELAY \
  Nop()  //('Nop()' is a single cycle null instruction for the C18 compiler,
         //include multiple times if required to provide time for the card to
         //drive the DO line if it needs to)

//###############################################
//##### END OF USING MICROCHIP C18 COMPILER #####
//###############################################
#endif  //#ifdef FFS_USING_MICROCHIP_C18_COMPILER

#ifdef MMCSD_USING_MICROCHIP_C30_COMPILER
//########################################
//##### USING MICROCHIP C30 COMPILER #####
//########################################

//----- CONTROL PINS -----
#define FFS_CE(state) \
  _LATF0 = state  // Chip select pin (set high on chip power-up)
#define FFC_DI \
  _RF2  // DO pin of SD card, DI pin of processor (used to check if pin is being
        // pulled low by card)

//----- SPI BUS CONTROL REGISTERS -----
#define FFS_SPI_BUF_FULL \
  IFS0bits.SPI1IF  //>0 when the SPI receive buffer contains a received byte,
                   //also signifying that transmit is complete
#define FFS_SPI_TX_BYTE(data) \
  IFS0bits.SPI1IF = 0;        \
  SPI1BUF =                   \
      data  // Macro to write a byte and start transmission over the SPI bus
#define FFS_SPI_RX_BYTE_BUFFER \
  SPI1BUF  // Register to read last received byte from

//----- SPI BUS SPEED -----
// SPI BUS LOW SPEED (Min 100KHz, Max 400KHz)  (Min speed required for ACDM41
// initalisation)
//- Adjust as required for the microcontroller and instruction clock frequency
//being used Prescallers 16:1, 8:1 = 312.5KHz
#define SPI_BUS_SET_TO_LOW_SPEED \
  SPI1STATbits.SPIEN = 0;        \
  SPI1CON1bits.PPRE1 = 0;        \
  SPI1CON1bits.PPRE0 = 1;        \
  SPI1CON1bits.SPRE2 = 0;        \
  SPI1CON1bits.SPRE1 = 0;        \
  SPI1CON1bits.SPRE0 = 0;        \
  SPI1STATbits.SPIEN = 1;

// SPI BUS FULL SPEED FOR MMC CARD (Max 20MHz)
//- Adjust as required for the microcontroller and instruction clock frequency
//being used Prescallers 4:1 1:1 = 10MHz (max possible from this device)
#define SPI_BUS_SET_TO_FULL_SPEED_MMC \
  SPI1STATbits.SPIEN = 0;             \
  SPI1CON1bits.PPRE1 = 1;             \
  SPI1CON1bits.PPRE0 = 0;             \
  SPI1CON1bits.SPRE2 = 1;             \
  SPI1CON1bits.SPRE1 = 1;             \
  SPI1CON1bits.SPRE0 = 1;             \
  SPI1STATbits.SPIEN = 1;

// SPI BUS FULL SPEED FOR SD CARD (Max 25MHz)
//- Adjust as required for the microcontroller and instruction clock frequency
//being used Prescallers 4:1 1:1 = 10MHz (max possible from this device)
#define SPI_BUS_SET_TO_FULL_SPEED_SD \
  SPI1STATbits.SPIEN = 0;            \
  SPI1CON1bits.PPRE1 = 1;            \
  SPI1CON1bits.PPRE0 = 0;            \
  SPI1CON1bits.SPRE2 = 1;            \
  SPI1CON1bits.SPRE1 = 1;            \
  SPI1CON1bits.SPRE0 = 1;            \
  SPI1STATbits.SPIEN = 1;

//----- CARD DETECT PIN -----
//(Defined like this so the pin can be connected to an input latch IC instead of
//directly to the processor if desired)
#define FFS_CD_PIN_REGISTER PORTG
#define FFS_CD_PIN_BIT 0x01  //(0x0008, 0x0004, 0x0002, 0x0001 etc)
//#define	FFS_CD_PIN_FUNCTION
////Optional function to call to get the FFS_RESET_PIN_REGISTER.  Comment out if
//not requried
//#define	FFS_CD_PIN_NC
////Include if card detect pin is normally closed (breaks when card inserted) or
//comment out if normally open (0V common assumed)

//----- WRITE PROTECT PIN -----
//(Defined like this so the pin can be connected to an input latch IC instead of
//directly to the processor if desired)
#define FFS_WP_PIN_REGISTER \
  PORTG  // To disable write protect pin checking comment out this define
#define FFS_WP_PIN_BIT \
  0x02  //(0x80, 0x40, 0x20, 0x10, 0x08, 0x04, 0x02 or 0x01)
//#define	FFS_WP_PIN_FUNCTION
////Optional function to call to get the FFS_RESET_PIN_REGISTER.  Comment out if
//not requried
#define FFS_WP_PIN_NC  // Include if card detect pin is normally closed (breaks
                       // when write protect is active) or comment out if
                       // normally open (closes when write protect is active)(0V
                       // common assumed)

//----- MISC -----
#define FFS_DRIVER_GEN_512_BYTE_BUFFER \
  ffs_general_buffer  // This general buffer is used by routines and may be the
                      // same as the buffer that the application uses to read and
                      // write data from and to the card if ram is limited

#define DO_BUSY_STATE_ACCESS_DELAY \
  Nop();                           \
  Nop()  //('Nop()' is a single cycle null instruction for the C30 compiler,
         //include multiple times if required to provide time for the card to
         //drive the DO line if it needs to)

//###############################################
//##### END OF USING MICROCHIP C30 COMPILER #####
//###############################################
#endif  //#ifdef FFS_USING_MICROCHIP_C30_COMPILER

#ifdef MMCSD_USING_MICROCHIP_C32_COMPILER
//########################################
//##### USING MICROCHIP C32 COMPILER #####
//########################################

//~~~~ WARNING - C32 COMPILER SPI_CHANNEL# DEFINITIONS ARE NON LOGICAL!
//SPI_CHANNEL2 = SPI1 etc ~~~~~

//----- CONTROL PINS -----
#define FFS_CE(state)           \
  (state ? mPORTFSetBits(BIT_1) \
         : mPORTFClearBits(     \
               BIT_1))  // Chip select pin (set high on chip power-up)
#define FFC_DI \
  mPORTGReadBits(BIT_7)  // DO pin of SD card, DI pin of processor (used to
                         // check if pin is being pulled low by card)

//----- SPI BUS CONTROL REGISTERS -----
#define FFS_SPI_BUF_FULL \
  SpiChnDataRdy(2)  //>0 when the SPI receive buffer contains a received byte,
                    //also signifying that transmit is complete
#define FFS_SPI_TX_BYTE(data) \
  WriteSPI2((unsigned int)data)  // Macro to write a byte and start transmission
                                 // over the SPI bus
#define FFS_SPI_RX_BYTE_BUFFER \
  (BYTE) ReadSPI2()  // Register to read last received byte from

//----- SPI BUS SPEED -----
// SPI BUS LOW SPEED (Min 100KHz, Max 400KHz)  (Min speed required for ACDM41
// initalisation)
//- Adjust as required for the microcontroller and instruction clock frequency
//being used Prescallers 16:1, 8:1 = 312.5KHz
#define SPI_BUS_SET_TO_LOW_SPEED                                    \
  SpiChnOpen(2, (SPI_CON_MSTEN | SPI_OPEN_CKE_REV | SPI_CON_MODE8), \
             50);  // 20MHz fpb / 50 = 400kHz

// SPI BUS FULL SPEED FOR MMC CARD (Max 20MHz)
//- Adjust as required for the microcontroller and instruction clock frequency
//being used Prescallers 4:1 1:1 = 10MHz (max possible from this device)
#define SPI_BUS_SET_TO_FULL_SPEED_MMC                        \
  SpiChnOpen(                                                \
      2, (SPI_CON_MSTEN | SPI_OPEN_CKE_REV | SPI_CON_MODE8), \
      2);  // 20MHz fpb / 2 = 10MHz (fastest possible for 20MHz fpb - PIC32 can
           // provide faster fbp, so adjust if using faster fpb)

// SPI BUS FULL SPEED FOR SD CARD (Max 25MHz)
//- Adjust as required for the microcontroller and instruction clock frequency
//being used Prescallers 4:1 1:1 = 10MHz (max possible from this device)
#define SPI_BUS_SET_TO_FULL_SPEED_SD                         \
  SpiChnOpen(                                                \
      2, (SPI_CON_MSTEN | SPI_OPEN_CKE_REV | SPI_CON_MODE8), \
      2);  // 20MHz fpb / 2 = 10MHz (fastest possible for 20MHz fpb - PIC32 can
           // provide faster fbp, so adjust if using faster fpb)

//----- CARD DETECT PIN -----
//(Defined like this so the pin can be connected to an input latch IC instead of
//directly to the processor if desired)
#define FFS_CD_PIN_REGISTER mPORTFRead()
#define FFS_CD_PIN_BIT 0x0001  //(0x0008, 0x0004, 0x0002, 0x0001 etc)
//#define	FFS_CD_PIN_FUNCTION
////Optional function to call to get the FFS_RESET_PIN_REGISTER.  Comment out if
//not requried
//#define	FFS_CD_PIN_NC
////Include if card detect pin is normally closed (breaks when card inserted) or
//comment out if normally open (0V common assumed)

//----- WRITE PROTECT PIN -----
//(Defined like this so the pin can be connected to an input latch IC instead of
//directly to the processor if desired) #define	FFS_WP_PIN_REGISTER
//mPORTFRead			//To disable write protect pin checking comment
//out this define
#define FFS_WP_PIN_BIT \
  0x0002  //(0x80, 0x40, 0x20, 0x10, 0x08, 0x04, 0x02 or 0x01)
//#define	FFS_WP_PIN_FUNCTION
////Optional function to call to get the FFS_RESET_PIN_REGISTER.  Comment out if
//not requried
#define FFS_WP_PIN_NC  // Include if card detect pin is normally closed (breaks
                       // when write protect is active) or comment out if
                       // normally open (closes when write protect is active)(0V
                       // common assumed)

//----- MISC -----
#define FFS_DRIVER_GEN_512_BYTE_BUFFER \
  ffs_general_buffer  // This general buffer is used by routines and may be the
                      // same as the buffer that the application uses to read and
                      // write data from and to the card if ram is limited

#define DO_BUSY_STATE_ACCESS_DELAY \
  Nop();                           \
  Nop()  //('Nop()' is a single cycle null instruction for the C30 compiler,
         //include multiple times if required to provide time for the card to
         //drive the DO line if it needs to)

//###############################################
//##### END OF USING MICROCHIP C32 COMPILER #####
//###############################################
#endif  //#ifdef MMCSD_USING_MICROCHIP_C32_COMPILER

#ifdef MMCSD_USING_CROSSWORKS_ARM_COMPILER
//#########################################
//##### USING CROSSWORKS ARM COMPILER #####
//#########################################

#ifdef MMCSD_C
BYTE spi_rx_dummy_reg;  // We need this register for the FFS_SPI_TX_BYTE define
#endif

//----- CONTROL PINS -----
#define FFS_CE(state)             \
  (state ? (FIO0SET = 0x40000000) \
         : (FIO0CLR =             \
                0x40000000))  // Chip select pin (set high on chip power-up)
#define FFC_DI \
  (FIO0PIN & 0x00020000)  // DO pin of SD card, DI pin of processor (used to
                          // check if pin is being pulled low by card)

//----- SPI BUS CONTROL REGISTERS -----
#define FFS_SPI_BUF_FULL \
  (SSP0SR & SSP0SR_RNE)  //>0 when the SPI receive buffer contains a received
                         //byte, also signifying that transmit is complete
#define FFS_SPI_TX_BYTE(data)                            \
  while (SSP0SR & SSP0SR_RNE) spi_rx_dummy_reg = SSP0DR; \
  SSP0DR =                                               \
      data  // Macro to write a byte and start transmission over the SPI bus
            // (the check for empty shouldn't strictly be required, but with this
            // part not including it can cause occasional failure)
#define FFS_SPI_RX_BYTE_BUFFER \
  (BYTE) SSP0DR  // Register to read last received byte from

//----- SPI BUS SPEED -----
// SPI BUS LOW SPEED (Min 100KHz, Max 400KHz)  (Min speed required for ACDM41
// initalisation)
//- Adjust as required for the microcontroller and instruction clock frequency
//being used
//#define	SPI_BUS_SET_TO_LOW_SPEED	S0SPCCR = 150		//SPI PCLK
//= 60MHz.  400KHz = 2.5uS = 150
#define SPI_BUS_SET_TO_LOW_SPEED \
  SSP0CPSR = 150  // SPI PCLK = 60MHz.  400KHz = 2.5uS = 150

// SPI BUS FULL SPEED FOR MMC CARD (Max 20MHz)
//- Adjust as required for the microcontroller and instruction clock frequency
//being used
//#define	SPI_BUS_SET_TO_FULL_SPEED_MMC	S0SPCCR = 8		//SPI
//PCLK = 60MHz.  20MHz = 50nS = 8(min allowed) = 7.5MHz actual speed
#define SPI_BUS_SET_TO_FULL_SPEED_MMC \
  SSP0CR1 = 0;                        \
  SSP0CPSR = 4;                       \
  SSP1CR1 = SSP0CR1_SSE  // SPI PCLK = 60MHz.  4 = 15MHz actual speed

// SPI BUS FULL SPEED FOR SD CARD (Max 25MHz)
//- Adjust as required for the microcontroller and instruction clock frequency
//being used
//#define	SPI_BUS_SET_TO_FULL_SPEED_SD	S0SPCCR = 8		//SPI
//PCLK = 60MHz.  25MHz = 40nS = 8(min allowed) = 7.5MHz actual speed
#define SPI_BUS_SET_TO_FULL_SPEED_SD \
  SSP0CR1 = 0;                       \
  SSP0CPSR = 4;                      \
  SSP0CR1 = SSP0CR1_SSE  // SPI PCLK = 60MHz.  4 = 15MHz actual speed

//----- CARD DETECT PIN -----
//(Defined like this so the pin can be connected to an input latch IC instead of
//directly to the processor if desired)
#define FFS_CD_PIN_REGISTER FIO0PIN
#define FFS_CD_PIN_BIT 0x00080000
//#define	FFS_CD_PIN_FUNCTION
////Optional function to call to get the FFS_RESET_PIN_REGISTER.  Comment out if
//not requried
//#define	FFS_CD_PIN_NC
////Include if card detect pin is normally closed (breaks when card inserted) or
//comment out if normally open (0V common assumed)

//----- WRITE PROTECT PIN -----
//(Defined like this so the pin can be connected to an input latch IC instead of
//directly to the processor if desired) #define	FFS_WP_PIN_REGISTER
//PORTG			//To disable write protect pin checking comment out this
//define #define	FFS_WP_PIN_BIT				0x02
////(0x80, 0x40, 0x20, 0x10, 0x08, 0x04, 0x02 or 0x01) #define
//FFS_WP_PIN_FUNCTION							//Optional
//function to call to get the FFS_RESET_PIN_REGISTER.  Comment out if not
//requried #define	FFS_WP_PIN_NC
////Include if card detect pin is normally closed (breaks when write protect is
//active) or comment out if normally open (closes when write protect is
// active)(0V common assumed)
//----- MISC -----
#define FFS_DRIVER_GEN_512_BYTE_BUFFER \
  ffs_general_buffer  // This general buffer is used by routines and may be the
                      // same as the buffer that the application uses to read and
                      // write data from and to the card if ram is limited

#define DO_BUSY_STATE_ACCESS_DELAY \
  __asm("nop");                    \
  __asm("nop");                    \
  __asm("nop");                    \
  __asm("nop");                    \
  __asm("nop");                    \
  __asm("nop");                    \
  __asm("nop");                    \
  __asm("nop");                    \
  __asm("nop");                    \
  __asm("nop");                    \
  __asm("nop");                    \
  __asm("nop")  //('Nop()' is a single cycle null instruction, include multiple
                //times if required to provide time for the card to drive the DO
                //line if it needs to)

//################################################
//##### END OF USING CROSSWORKS ARM COMPILER #####
//################################################
#endif  //#ifdef MMCSD_USING_CROSSWORKS_ARM_COMPILER

#ifdef RULOS_ARM
#include "core/hardware.h"
#include "stm32f3xx_ll_spi.h"

#define FFS_CE(state) gpio_set_or_clr(GPIO_B6, state)
#define FFC_DI gpio_is_set(GPIO_B4)
#define FFS_SPI_TX_BYTE(data) LL_SPI_TransmitData8(SPI1, data)
#define FFS_SPI_RX_BYTE_BUFFER LL_SPI_ReceiveData8(SPI1)
#define FFS_SPI_BUF_FULL LL_SPI_IsActiveFlag_BSY(SPI1)

#define SPI_BUS_SET_TO_LOW_SPEED
#define SPI_BUS_SET_TO_FULL_SPEED_SD
#define SPI_BUS_SET_TO_FULL_SPEED_MMC
#define FFS_DRIVER_GEN_512_BYTE_BUFFER \
  ffs_general_buffer  // This general buffer is used by driver functions
#define DO_BUSY_STATE_ACCESS_DELAY __NOP()
#endif  // RULOS_ARM

//---------------------------------------------
//----- PROCESS CARD STATE MACHINE STATES -----
//---------------------------------------------
typedef enum _FFS_PROCESS_STATE {
  FFS_PROCESS_NO_CARD,
  FFS_PROCESS_WAIT_FOR_CARD_FULLY_INSERTED,
  FFS_PROCESS_CARD_INITIALSIED
} FFS_PROCESS_STATE;

#endif

//*******************************
//*******************************
//********** FUNCTIONS **********
//*******************************
//*******************************
#ifdef MMCSD_C
//-----------------------------------
//----- INTERNAL ONLY FUNCTIONS -----
//-----------------------------------
BYTE ffs_check_command_response_byte(BYTE mask, BYTE data_requried);

//-----------------------------------------
//----- INTERNAL & EXTERNAL FUNCTIONS -----
//-----------------------------------------
//(Also defined below as extern)
void ffs_process(void);
BYTE ffs_is_card_present(void);
void ffs_read_sector_to_buffer(DWORD sector_lba);
void ffs_write_sector_from_buffer(DWORD sector_lba);
BYTE ffs_write_byte(BYTE data);
WORD ffs_read_word(void);
BYTE ffs_read_byte(void);

#else
//------------------------------
//----- EXTERNAL FUNCTIONS -----
//------------------------------
extern void ffs_process(void);
extern BYTE ffs_is_card_present(void);
extern void ffs_read_sector_to_buffer(DWORD sector_lba);
extern void ffs_write_sector_from_buffer(DWORD sector_lba);
extern BYTE ffs_write_byte(BYTE data);
extern WORD ffs_read_word(void);
extern BYTE ffs_read_byte(void);

#endif

//****************************
//****************************
//********** MEMORY **********
//****************************
//****************************
#ifdef MMCSD_C
//--------------------------------------------
//----- INTERNAL ONLY MEMORY DEFINITIONS -----
//--------------------------------------------
BYTE sm_ffs_process = FFS_PROCESS_NO_CARD;
WORD file_system_information_sector;
BYTE ffs_no_of_heads;
BYTE ffs_no_of_sectors_per_track;
DWORD ffs_no_of_partition_sectors;
BYTE card_is_high_capacity;
BYTE chk_cmd_response_data;

//--------------------------------------------------
//----- INTERNAL & EXTERNAL MEMORY DEFINITIONS -----
//--------------------------------------------------
//(Also defined below as extern)
WORD number_of_root_directory_sectors;  // Only used by FAT16, 0 for FAT32
BYTE ffs_buffer_needs_writing_to_card;
DWORD ffs_buffer_contains_lba = 0xffffffff;
DWORD fat1_start_sector;
DWORD root_directory_start_sector_cluster;  // Start sector for FAT16, start
                                            // clustor for FAT32
DWORD data_area_start_sector;
BYTE disk_is_fat_32;
BYTE sectors_per_cluster;
DWORD last_found_free_cluster;
DWORD sectors_per_fat;
BYTE active_fat_table_flags;
DWORD read_write_directory_last_lba;
WORD read_write_directory_last_entry;
BYTE ffs_card_write_protected = 0;

#else
//---------------------------------------
//----- EXTERNAL MEMORY DEFINITIONS -----
//---------------------------------------
extern WORD number_of_root_directory_sectors;  // Only used by FAT16, 0 for
                                               // FAT32
extern BYTE ffs_buffer_needs_writing_to_card;
extern DWORD ffs_buffer_contains_lba;
extern DWORD fat1_start_sector;
extern DWORD root_directory_start_sector_cluster;
extern DWORD data_area_start_sector;
extern BYTE disk_is_fat_32;
extern BYTE sectors_per_cluster;
extern DWORD last_found_free_cluster;
extern DWORD sectors_per_fat;
extern BYTE active_fat_table_flags;
extern DWORD read_write_directory_last_lba;
extern WORD read_write_directory_last_entry;
extern BYTE ffs_card_write_protected;

#endif
