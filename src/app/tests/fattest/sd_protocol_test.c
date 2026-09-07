#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "periph/sdcard2/fatfs_sd.c"

SPI_TypeDef sd_test_spi;
static Time now, started, spi_step;
static bool selected, force_busy, respond;
static unsigned card_type, ready_after, op_count, init_count;
static uint8_t command[6], response[8];
static unsigned command_len, response_len, response_pos;

Time clock_time_us(void) {
  return now;
}

static void advance(Time us) {
  now += us;
  // Bound a broken driver even when it keeps renewing or underflows a deadline.
  assert(now - started < time_sec(10));
}

void sd_test_idle(void) {
  advance(21000);
}

void gpio_set(int pin) {
  assert(pin == GPIO_B6);
  selected = false;
  command_len = response_len = response_pos = 0;
}

void gpio_clr(int pin) {
  assert(pin == GPIO_B6);
  selected = true;
}

static uint8_t exchange(uint8_t byte) {
  advance(spi_step);
  if (!selected) {
    return 0xff;
  }
  if (force_busy) {
    return 0;
  }
  if (response_pos < response_len) {
    return response[response_pos++];
  }
  if (respond && (command_len || (byte & 0xc0) == 0x40)) {
    command[command_len++] = byte;
    if (command_len == sizeof(command)) {
      unsigned cmd = command[0] & 63;
      command_len = response_pos = 0;
      response_len = 1;
      response[0] = 1;
      switch (cmd) {
        case CMD8:
          if (card_type == CT_SD2) {
            const uint8_t r7[] = {1, 0, 0, 1, 0xaa};
            memcpy(response, r7, sizeof(r7));
            response_len = sizeof(r7);
          } else {
            response[0] = 5;
          }
          break;
        case CMD55:
          response[0] = card_type == CT_MMC ? 5 : 1;
          break;
        case (ACMD41 & 0x7f):
        case CMD1:
          op_count++;
          response[0] = ready_after && op_count >= ready_after ? 0 : 1;
          break;
        case CMD58: {
          const uint8_t ocr[] = {0, 0x40, 0, 0, 0};
          memcpy(response, ocr, sizeof(ocr));
          response_len = sizeof(ocr);
          break;
        }
        case CMD16:
          response[0] = 0;
          break;
      }
    }
  }
  return 0xff;
}

void LL_SPI_TransmitData8(SPI_TypeDef *spi, uint8_t byte) {
  assert(spi == SPI1);
  spi->DR = exchange(byte);
}

uint8_t LL_SPI_ReceiveData8(SPI_TypeDef *spi) {
  assert(spi == SPI1);
  return spi->DR;
}

void TM_SPI_Init(void) {
  init_count++;
}
void TM_SPI_SetSlow(void) {
}
void TM_SPI_SetFast(void) {
}
void FATFS_DEBUG_SEND_USART(const char *msg) {
  (void)msg;
}
void TM_SPI_ReadMulti(SPI_TypeDef *spi, uint8_t *buffer, uint8_t dummy, uint32_t count) {
  (void)spi;
  memset(buffer, dummy, count);
}
void TM_SPI_WriteMulti(SPI_TypeDef *spi, uint8_t *buffer, uint32_t count) {
  (void)spi;
  (void)buffer;
  (void)count;
}

static void reset(Time start) {
  now = started = start;
  spi_step = 100;
  selected = force_busy = respond = false;
  card_type = CT_SD2;
  ready_after = op_count = init_count = 0;
  command_len = response_len = response_pos = 0;
  TM_FATFS_SD_Stat = STA_NOINIT;
  TM_FATFS_SD_CardType = 0;
}

static void test_timeouts(Time start) {
  reset(start);
  init_spi();
  assert(now - started == 21000 && init_count == 1);

  reset(start);
  selected = force_busy = true;
  spi_step = 1000;
  assert(!wait_ready(10));
  assert(now - started == 10000);

  reset(start);
  selected = force_busy = true;
  spi_step = 11000;
  assert(!wait_ready(10));
  assert(now - started == 11000);

  reset(start);
  selected = true;
  assert(wait_ready(10));
  assert(now - started == spi_step);

  reset(start);
  selected = true;
  spi_step = 11000;
  BYTE block[16];
  assert(!rcvr_datablock(block, sizeof(block)));
  assert(now - started >= 200000 && now - started < 211000);
}

static void test_initialization(Time start, unsigned type, bool succeeds) {
  reset(start);
  respond = true;
  card_type = type;
  ready_after = succeeds ? 3 : 0;
  DSTATUS status = TM_FATFS_SD_disk_initialize();
  assert(init_count == 1);
  if (succeeds) {
    assert(status == 0 && op_count == ready_after);
    assert(TM_FATFS_SD_CardType == (type == CT_SD2 ? CT_SD2 | CT_BLOCK : type));
    assert(now - started < 100000);
  } else {
    assert(status & STA_NOINIT);
    assert(now - started >= time_msec(1000) && now - started < time_msec(1100));
  }
}

int main(void) {
  const Time starts[] = {0, UINT32_MAX - 5000};
  const unsigned types[] = {CT_SD2, CT_SD1, CT_MMC};
  for (unsigned i = 0; i < sizeof(starts) / sizeof(starts[0]); i++) {
    test_timeouts(starts[i]);
    for (unsigned j = 0; j < sizeof(types) / sizeof(types[0]); j++) {
      test_initialization(starts[i], types[j], false);
      test_initialization(starts[i], types[j], true);
    }
  }
  puts("sd protocol: local deadlines, late polls, rollover, and card initialization passed");
}
