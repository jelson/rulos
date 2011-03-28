#ifndef _SDCARD_H
#define _SDCARD_H

#include "rocket.h"
#include "hardware_audio.h"
#include "seqmacro.h"
#include "spi.h"

// Memory is too scarce to have separate callers with their own buffers;
// so we allocate a single block buffer, and share it with all callers.
#define SDBUFSIZE	512
#define SDCRCSIZE	2

typedef struct
{
	HALSPIHandler handler;
	int numclocks_bytes;
	ActivationFuncPtr done_func;
	void *done_data;
} BunchaClocks;

typedef struct {
	uint16_t blocksize;
	SPI spi;
	SPICmd spic;
	BunchaClocks bunchaClocks;
	uint8_t read_cmdseq[6];
	r_bool transaction_open;
	r_bool error;
} SDCard;

void sdc_init(SDCard *sdc);

void sdc_reset_card(SDCard *sdc, ActivationFuncPtr done_func, void *done_data);

r_bool sdc_start_transaction(
	SDCard *sdc,
	uint32_t offset,
	uint8_t *buffer,
	uint16_t buflen,
	ActivationFuncPtr done_func,
	void *done_data);
	// FALSE if sdc was busy.

r_bool sdc_is_error(SDCard *sdc);
	// only valid during a transaction.

void sdc_continue_transaction(
	SDCard *sdc,
	uint8_t *buffer,
	uint16_t buflen,
	ActivationFuncPtr done_func,
	void *done_data);

void sdc_end_transaction(SDCard *sdc, ActivationFuncPtr done_func, void *done_data);

#endif // _SDCARD_H
