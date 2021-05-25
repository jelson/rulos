/*-----------------------------------------------------------------------*/
/* SPI controls (Platform dependent)                                     */
/*-----------------------------------------------------------------------*/

#include "core/rulos.h"

#include "periph/fatfs/diskio.h"
#include "periph/sdcard2/fatfs_rulos.h"

/* MMC/SD command */
#define CMD0	(0)			/* GO_IDLE_STATE */
#define CMD1	(1)			/* SEND_OP_COND (MMC) */
#define	ACMD41	(0x80+41)	/* SEND_OP_COND (SDC) */
#define CMD8	(8)			/* SEND_IF_COND */
#define CMD9	(9)			/* SEND_CSD */
#define CMD10	(10)		/* SEND_CID */
#define CMD12	(12)		/* STOP_TRANSMISSION */
#define ACMD13	(0x80+13)	/* SD_STATUS (SDC) */
#define CMD16	(16)		/* SET_BLOCKLEN */
#define CMD17	(17)		/* READ_SINGLE_BLOCK */
#define CMD18	(18)		/* READ_MULTIPLE_BLOCK */
#define CMD23	(23)		/* SET_BLOCK_COUNT (MMC) */
#define	ACMD23	(0x80+23)	/* SET_WR_BLK_ERASE_COUNT (SDC) */
#define CMD24	(24)		/* WRITE_BLOCK */
#define CMD25	(25)		/* WRITE_MULTIPLE_BLOCK */
#define CMD32	(32)		/* ERASE_ER_BLK_START */
#define CMD33	(33)		/* ERASE_ER_BLK_END */
#define CMD38	(38)		/* ERASE */
#define CMD55	(55)		/* APP_CMD */
#define CMD58	(58)		/* READ_OCR */

/* MMC card type flags (MMC_GET_TYPE) */
#define CT_MMC		0x01		/* MMC ver 3 */
#define CT_SD1		0x02		/* SD ver 1 */
#define CT_SD2		0x04		/* SD ver 2 */
#define CT_SDC		(CT_SD1|CT_SD2)	/* SD */
#define CT_BLOCK	0x08		/* Block addressing */
static volatile DSTATUS TM_FATFS_SD_Stat = STA_NOINIT;	/* Physical drive status */

static BYTE TM_FATFS_SD_CardType;			/* Card type flags */

/* Initialize MMC interface */
static void init_spi (void) {
	/* Init delay functions */
	TM_DELAY_Init();

	/* Init SPI */
	TM_SPI_Init();

	/* Set CS high */
	FATFS_CS_HIGH;

	/* Wait for stable */
	TM_DELAY_SetTime2(20);
	while (TM_DELAY_Time2() > 0) {
		__WFI();
	}
}


/* Receive multiple byte */
static void rcvr_spi_multi (
	BYTE *buff,		/* Pointer to data buffer */
	UINT btr		/* Number of bytes to receive (even number) */
)
{
	/* Read multiple bytes, send 0xFF as dummy */
	TM_SPI_ReadMulti(FATFS_SPI, buff, 0xFF, btr);
}


#if _USE_WRITE
/* Send multiple byte */
static void xmit_spi_multi (
	const BYTE *buff,	/* Pointer to the data */
	UINT btx			/* Number of bytes to send (even number) */
)
{
	/* Write multiple bytes */
	TM_SPI_WriteMulti(FATFS_SPI, (uint8_t *)buff, btx);
}
#endif


/*-----------------------------------------------------------------------*/
/* Wait for card ready                                                   */
/*-----------------------------------------------------------------------*/

static int wait_ready (	/* 1:Ready, 0:Timeout */
	UINT wt			/* Timeout [ms] */
)
{
	BYTE d;

	/* Set down counter */
	TM_DELAY_SetTime2(wt);

	do {
		d = TM_SPI_Send(FATFS_SPI, 0xFF);
	} while (d != 0xFF && TM_DELAY_Time2());	/* Wait for card goes ready or timeout */
	if (d == 0xFF) {
		FATFS_DEBUG_SEND_USART("wait_ready: OK");
	} else {
		FATFS_DEBUG_SEND_USART("wait_ready: timeout");
	}
	return (d == 0xFF) ? 1 : 0;
}



/*-----------------------------------------------------------------------*/
/* Deselect card and release SPI                                         */
/*-----------------------------------------------------------------------*/

static void deselect_card (void)
{
	FATFS_CS_HIGH;			/* CS = H */
	TM_SPI_Send(FATFS_SPI, 0xFF);			/* Dummy clock (force DO hi-z for multiple slave SPI) */
#if 0
	FATFS_DEBUG_SEND_USART("deselect: ok");
#endif
}



/*-----------------------------------------------------------------------*/
/* Select card and wait for ready                                        */
/*-----------------------------------------------------------------------*/

static int select_card (void)	/* 1:OK, 0:Timeout */
{
	TM_SPI_Send(FATFS_SPI, 0xFF);	/* Dummy clock (force DO enabled) */
	FATFS_CS_LOW;
	return 1;

#if 0
	if (wait_ready(500)) {
		FATFS_DEBUG_SEND_USART("select: OK");
		return 1;	/* OK */
	}
	FATFS_DEBUG_SEND_USART("select: no");
	deselect_card();

	return 0;	/* Timeout */
#endif
}



/*-----------------------------------------------------------------------*/
/* Receive a data packet from the MMC                                    */
/*-----------------------------------------------------------------------*/

static int rcvr_datablock (	/* 1:OK, 0:Error */
	BYTE *buff,			/* Data buffer */
	UINT btr			/* Data block length (byte) */
)
{
	BYTE token;

	//Timer1 = 200;

	FATFS_DEBUG_SEND_USART("rcvr_datablock: inside");
	TM_DELAY_SetTime2(200);
	do {							// Wait for DataStart token in timeout of 200ms
		token = TM_SPI_Send(FATFS_SPI, 0xFF);
		// This loop will take a time. Insert rot_rdq() here for multitask envilonment.
	} while ((token == 0xFF) && TM_DELAY_Time2());
	if (token == 0xFF) {
		FATFS_DEBUG_SEND_USART("rcvr_datablock: timed out");
		return 0;		// Function fails if invalid DataStart token or timeout
	}

	rcvr_spi_multi(buff, btr);		// Store trailing data to the buffer
	TM_SPI_Send(FATFS_SPI, 0xFF); TM_SPI_Send(FATFS_SPI, 0xFF);			// Discard CRC
	FATFS_DEBUG_SEND_USART("rcvr_datablock: success");
	return 1;						// Function succeeded
}



/*-----------------------------------------------------------------------*/
/* Send a data packet to the MMC                                         */
/*-----------------------------------------------------------------------*/

#if _USE_WRITE
static int xmit_datablock (	/* 1:OK, 0:Failed */
	const BYTE *buff,	/* Ponter to 512 byte data to be sent */
	BYTE token			/* Token */
)
{
	BYTE resp;

	FATFS_DEBUG_SEND_USART("xmit_datablock: inside");

#if 0
	if (!wait_ready(500)) {
		FATFS_DEBUG_SEND_USART("xmit_datablock: not ready");
		return 0;		/* Wait for card ready */
	}
#endif


	TM_SPI_Send(FATFS_SPI, token);					/* Send token */
	xmit_spi_multi(buff, 512);		/* Data */

	/* Dummy CRC */
	TM_SPI_Send(FATFS_SPI, 0xFF);
	TM_SPI_Send(FATFS_SPI, 0xFF);

	resp = TM_SPI_Send(FATFS_SPI, 0xFF);				/* Receive data resp */
	if ((resp & 0x1F) != 0x05)	{	/* Function fails if the data packet was not accepted */
		LOG("xmit_datablock: failed, got 0x%x", resp);
		return 0;
	}

	//FATFS_DEBUG_SEND_USART("xmit_datablock: success");
	return 1;
}
#endif


/*-----------------------------------------------------------------------*/
/* Send a command packet to the MMC                                      */
/*-----------------------------------------------------------------------*/

static BYTE send_cmd(		/* Return value: R1 resp (bit7==1:Failed to send) */
	BYTE cmd,		/* Command index */
	DWORD arg		/* Argument */
)
{
	BYTE n, res;

	//LOG("SD card: sending command %d", cmd);

	if (cmd & 0x80) {	/* Send a CMD55 prior to ACMD<n> */
		cmd &= 0x7F;
		res = send_cmd(CMD55, 0);
		if (res > 1) return res;
	}

	/* Select the card and wait for ready except to stop multiple block read */
	if (cmd != CMD12) {
		if (!select_card()) {
			LOG("card-select failed in send_cmd!");
			return 0xFF;
		}
	}

	/* Send command packet */
	TM_SPI_Send(FATFS_SPI, 0x40 | cmd);				/* Start + command index */
	TM_SPI_Send(FATFS_SPI, (BYTE)(arg >> 24));		/* Argument[31..24] */
	TM_SPI_Send(FATFS_SPI, (BYTE)(arg >> 16));		/* Argument[23..16] */
	TM_SPI_Send(FATFS_SPI, (BYTE)(arg >> 8));		/* Argument[15..8] */
	TM_SPI_Send(FATFS_SPI, (BYTE)arg);				/* Argument[7..0] */
	n = 0x01;										/* Dummy CRC + Stop */
	if (cmd == CMD0) n = 0x95;						/* Valid CRC for CMD0(0) */
	if (cmd == CMD8) n = 0x87;						/* Valid CRC for CMD8(0x1AA) */
	if (cmd == 41) n = 0x77;
	TM_SPI_Send(FATFS_SPI, n);

	/* Receive command resp */
	if (cmd == CMD12) {
		TM_SPI_Send(FATFS_SPI, 0xFF);					/* Diacard following one byte when CMD12 */
	}

	int retries = 100;

	if (cmd == 0) {
	  do {
	    res = TM_SPI_Send(FATFS_SPI, 0xFF);
	  } while (res != 0x1 && --retries);
	} else {
	  do {
	    res = TM_SPI_Send(FATFS_SPI, 0xFF);
	  } while ((res & 0x80) && --retries);
	}

	return res;
}

#if 0
// Disabled in RULOS.
void TM_FATFS_InitPins(void) {
	/* CS pin */
	TM_GPIO_Init(FATFS_CS_PORT, FATFS_CS_PIN, TM_GPIO_Mode_OUT, TM_GPIO_OType_PP, TM_GPIO_PuPd_UP, TM_GPIO_Speed_Low);

	/* Detect pin */
#if FATFS_USE_DETECT_PIN > 0
	TM_GPIO_Init(FATFS_USE_DETECT_PIN_PORT, FATFS_USE_DETECT_PIN_PIN, TM_GPIO_Mode_IN, TM_GPIO_OType_PP, TM_GPIO_PuPd_UP, TM_GPIO_Speed_Low);
#endif

	/* Write protect pin */
#if FATFS_USE_WRITEPROTECT_PIN > 0
	TM_GPIO_Init(FATFS_USE_WRITEPROTECT_PIN_PORT, FATFS_USE_WRITEPROTECT_PIN_PIN, TM_GPIO_Mode_IN, TM_GPIO_OType_PP, TM_GPIO_PuPd_UP, TM_GPIO_Speed_Low);
#endif
}
#endif

uint8_t TM_FATFS_Detect(void) {
#if FATFS_USE_DETECT_PIN > 0
	return !TM_GPIO_GetInputPinValue(FATFS_USE_DETECT_PIN_PORT, FATFS_USE_DETECT_PIN_PIN);
#else
	return 1;
#endif
}

uint8_t TM_FATFS_WriteEnabled(void) {
#if FATFS_USE_WRITEPROTECT_PIN > 0
	return !TM_GPIO_GetInputPinValue(FATFS_USE_WRITEPROTECT_PIN_PORT, FATFS_USE_WRITEPROTECT_PIN_PIN);
#else
	return 1;
#endif
}

DSTATUS TM_FATFS_SD_disk_initialize (void) {
	BYTE n, cmd, ty, ocr[4];

	// RULOS: commented out; this is done in init_spi
	//TM_FATFS_InitPins();

	init_spi();

	if (!TM_FATFS_Detect()) {
		return STA_NODISK;
	}

	TM_SPI_SetSlow();

	for (n = 10; n; n--) {
		TM_SPI_Send(FATFS_SPI, 0xFF);
	}
	ty = 0;
	if (send_cmd(CMD0, 0) == 1) {				/* Put the card SPI/Idle state */
		TM_DELAY_SetTime2(1000);				/* Initialization timeout = 1 sec */
		if (send_cmd(CMD8, 0x1AA) == 1) {	/* SDv2? */
			for (n = 0; n < 4; n++) {
				ocr[n] = TM_SPI_Send(FATFS_SPI, 0xFF);	/* Get 32 bit return value of R7 resp */
			}
			if (ocr[2] == 0x01 && ocr[3] == 0xAA) {				/* Is the card supports vcc of 2.7-3.6V? */
				while (TM_DELAY_Time2() && send_cmd(ACMD41, 1UL << 30)) ;	/* Wait for end of initialization with ACMD41(HCS) */
				if (TM_DELAY_Time2() && send_cmd(CMD58, 0) == 0) {		/* Check CCS bit in the OCR */
					for (n = 0; n < 4; n++) {
						ocr[n] = TM_SPI_Send(FATFS_SPI, 0xFF);
					}
					ty = (ocr[0] & 0x40) ? CT_SD2 | CT_BLOCK : CT_SD2;	/* Card id SDv2 */
				}
			}
		} else {	/* Not SDv2 card */
			if (send_cmd(ACMD41, 0) <= 1) 	{	/* SDv1 or MMC? */
				ty = CT_SD1; cmd = ACMD41;	/* SDv1 (ACMD41(0)) */
			} else {
				ty = CT_MMC; cmd = CMD1;	/* MMCv3 (CMD1(0)) */
			}
			while (TM_DELAY_Time2() && send_cmd(cmd, 0));			/* Wait for end of initialization */
			if (TM_DELAY_Time2() || send_cmd(CMD16, 512) != 0) {	/* Set block length: 512 */
				ty = 0;
			}
		}
	}
	TM_FATFS_SD_CardType = ty;	/* Card type */
	deselect_card();

	if (ty) {			/* OK */
		TM_FATFS_SD_Stat &= ~STA_NOINIT;	/* Clear STA_NOINIT flag */
	} else {			/* Failed */
		TM_FATFS_SD_Stat = STA_NOINIT;
	}

	if (!TM_FATFS_WriteEnabled()) {
		TM_FATFS_SD_Stat |= STA_PROTECT;
	} else {
		TM_FATFS_SD_Stat &= ~STA_PROTECT;
	}

	TM_SPI_SetFast();
	return TM_FATFS_SD_Stat;
}



/*-----------------------------------------------------------------------*/
/* Get Disk Status                                                       */
/*-----------------------------------------------------------------------*/

DSTATUS TM_FATFS_SD_disk_status (void) {

	/* Check card detect pin if enabled */
	if (!TM_FATFS_Detect()) {
		return STA_NOINIT;
	}

	/* Check if write is enabled */
	if (!TM_FATFS_WriteEnabled()) {
		TM_FATFS_SD_Stat |= STA_PROTECT;
	} else {
		TM_FATFS_SD_Stat &= ~STA_PROTECT;
	}

	return TM_FATFS_SD_Stat;	/* Return disk status */
}



/*-----------------------------------------------------------------------*/
/* Read Sector(s)                                                        */
/*-----------------------------------------------------------------------*/

DRESULT TM_FATFS_SD_disk_read (
	BYTE *buff,		/* Data buffer to store read data */
	DWORD sector,	/* Sector address (LBA) */
	UINT count		/* Number of sectors to read (1..128) */
)
{
	//LOG("disk read: sector %ld, count %lu", sector, count);

	if (!TM_FATFS_Detect() || (TM_FATFS_SD_Stat & STA_NOINIT)) {
		return RES_NOTRDY;
	}

	if (!(TM_FATFS_SD_CardType & CT_BLOCK)) {
		sector *= 512;	/* LBA ot BA conversion (byte addressing cards) */
	}

	if (count == 1) {	/* Single sector read */
		/* READ_SINGLE_BLOCK */
		int retval;
		int retries = 20;
		do {
			retval = send_cmd(CMD17, sector);
			retries--;
		} while (retries > 0 && retval != 0);

		if (retval != 0) {
			LOG("warning: got non-zero response %d to CMD17", retval);
		} else {
			if (rcvr_datablock(buff, 512)) {
				count = 0;
			}
		}
	} else {				/* Multiple sector read */
		/* READ_MULTIPLE_BLOCK */
		//LOG("multi sector read");
		int retval = send_cmd(CMD18, sector);
		if (retval != 0) {
			LOG("warning: got non-zero response %d to CMD18", retval);
		} else {
			do {
				if (!rcvr_datablock(buff, 512)) {
					break;
				}
				buff += 512;
			} while (--count);

			/* STOP_TRANSMISSION */
			send_cmd(CMD12, 0);
		}
	}
	deselect_card();

	return count ? RES_ERROR : RES_OK;	/* Return result */
}



/*-----------------------------------------------------------------------*/
/* Write Sector(s)                                                       */
/*-----------------------------------------------------------------------*/

#if _USE_WRITE
DRESULT TM_FATFS_SD_disk_write (
	const BYTE *buff,	/* Data to be written */
	DWORD sector,		/* Sector address (LBA) */
	UINT count			/* Number of sectors to write (1..128) */
)
{
	//LOG("writing blocks %ld-%ld", sector, sector+count);
	FATFS_DEBUG_SEND_USART("disk_write: inside");
	if (!TM_FATFS_Detect()) {
		return RES_ERROR;
	}
	if (!TM_FATFS_WriteEnabled()) {
		FATFS_DEBUG_SEND_USART("disk_write: Write protected!!! \n---------------------------------------------");
		return RES_WRPRT;
	}
	if (TM_FATFS_SD_Stat & STA_NOINIT) {
		return RES_NOTRDY;	/* Check drive status */
	}
	if (TM_FATFS_SD_Stat & STA_PROTECT) {
		return RES_WRPRT;	/* Check write protect */
	}

	if (!(TM_FATFS_SD_CardType & CT_BLOCK)) {
		sector *= 512;	/* LBA ==> BA conversion (byte addressing cards) */
	}

	if (count == 1) {	/* Single sector write */
		//LOG("single sector write");
		/* WRITE_BLOCK */
		int retval = send_cmd(CMD24, sector);
		if (retval != 0) {
			LOG("warning: got non-zero response %d to CMD24", retval);
		} else {
			if (xmit_datablock(buff, 0xFE)) {
				count = 0;
			} else {
				LOG("error writing datablock");
			}
		}
		//LOG("single sector write done");
	} else {				/* Multiple sector write */
		//LOG("multi sector write");
		if (TM_FATFS_SD_CardType & CT_SDC) send_cmd(ACMD23, count);	/* Predefine number of sectors */
		/* WRITE_MULTIPLE_BLOCK */
		int retval = send_cmd(CMD25, sector);
		if (retval != 0) {
			LOG("warning: got non-zero response %d to CMD25", retval);
		} else {
			do {
				if (!xmit_datablock(buff, 0xFC)) {
					break;
				}
				buff += 512;

				if (!wait_ready(50)) {
					LOG("timed out waiting for multiblock ack");
					goto done;
				}
			} while (--count);


			/* STOP_TRAN token */
			TM_SPI_Send(FATFS_SPI, 0xFD);
		}
	}

	// wait for the write to complete
	if (!wait_ready(50)) {
		LOG("timed out waiting for block ack");
	}

done:
	deselect_card();

	return count ? RES_ERROR : RES_OK;	/* Return result */
}
#endif


/*-----------------------------------------------------------------------*/
/* Miscellaneous Functions                                               */
/*-----------------------------------------------------------------------*/

#if _USE_IOCTL
DRESULT TM_FATFS_SD_disk_ioctl (
	BYTE cmd,		/* Control code */
	void *buff		/* Buffer to send/receive control data */
)
{
	DRESULT res;
	BYTE n, csd[16];
	DWORD *dp, st, ed, csize;

	if (TM_FATFS_SD_Stat & STA_NOINIT) {
		return RES_NOTRDY;	/* Check if drive is ready */
	}
	if (!TM_FATFS_Detect()) {
		return RES_NOTRDY;
	}

	res = RES_ERROR;

	switch (cmd) {
	case CTRL_SYNC :		/* Wait for end of internal write process of the drive */
		if (select_card()) res = RES_OK;
		break;

	case GET_SECTOR_COUNT :	/* Get drive capacity in unit of sector (DWORD) */
		if ((send_cmd(CMD9, 0) == 0) && rcvr_datablock(csd, 16)) {
			if ((csd[0] >> 6) == 1) {	/* SDC ver 2.00 */
				csize = csd[9] + ((WORD)csd[8] << 8) + ((DWORD)(csd[7] & 63) << 16) + 1;
				*(DWORD*)buff = csize << 10;
			} else {					/* SDC ver 1.XX or MMC ver 3 */
				n = (csd[5] & 15) + ((csd[10] & 128) >> 7) + ((csd[9] & 3) << 1) + 2;
				csize = (csd[8] >> 6) + ((WORD)csd[7] << 2) + ((WORD)(csd[6] & 3) << 10) + 1;
				*(DWORD*)buff = csize << (n - 9);
			}
			res = RES_OK;
		}
		break;

	case GET_BLOCK_SIZE :	/* Get erase block size in unit of sector (DWORD) */
		if (TM_FATFS_SD_CardType & CT_SD2) {	/* SDC ver 2.00 */
			if (send_cmd(ACMD13, 0) == 0) {	/* Read SD status */
				TM_SPI_Send(FATFS_SPI, 0xFF);
				if (rcvr_datablock(csd, 16)) {				/* Read partial block */
					for (n = 64 - 16; n; n--) TM_SPI_Send(FATFS_SPI, 0xFF);	/* Purge trailing data */
					*(DWORD*)buff = 16UL << (csd[10] >> 4);
					res = RES_OK;
				}
			}
		} else {					/* SDC ver 1.XX or MMC */
			if ((send_cmd(CMD9, 0) == 0) && rcvr_datablock(csd, 16)) {	/* Read CSD */
				if (TM_FATFS_SD_CardType & CT_SD1) {	/* SDC ver 1.XX */
					*(DWORD*)buff = (((csd[10] & 63) << 1) + ((WORD)(csd[11] & 128) >> 7) + 1) << ((csd[13] >> 6) - 1);
				} else {					/* MMC */
					*(DWORD*)buff = ((WORD)((csd[10] & 124) >> 2) + 1) * (((csd[11] & 3) << 3) + ((csd[11] & 224) >> 5) + 1);
				}
				res = RES_OK;
			}
		}
		break;

	case CTRL_TRIM :	/* Erase a block of sectors (used when _USE_ERASE == 1) */
		if (!(TM_FATFS_SD_CardType & CT_SDC)) break;				/* Check if the card is SDC */
		if (TM_FATFS_SD_disk_ioctl(MMC_GET_CSD, csd)) break;	/* Get CSD */
		if (!(csd[0] >> 6) && !(csd[10] & 0x40)) break;	/* Check if sector erase can be applied to the card */
		dp = buff; st = dp[0]; ed = dp[1];				/* Load sector block */
		if (!(TM_FATFS_SD_CardType & CT_BLOCK)) {
			st *= 512; ed *= 512;
		}
		if (send_cmd(CMD32, st) == 0 && send_cmd(CMD33, ed) == 0 && send_cmd(CMD38, 0) == 0 && wait_ready(30000))	/* Erase sector block */
			res = RES_OK;	/* FatFs does not check result of this command */
		break;

	default:
		res = RES_PARERR;
	}

	deselect_card();

	return res;
}
#endif
