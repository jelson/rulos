#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "periph/sdcard2/fatfs_sd.c"

SPI_TypeDef sd_test_spi;
static Time now, started, spi_step;
static bool selected, force_busy, respond;
static bool transfer_ok;
static unsigned read_count, write_count;
static bool writing, programming, data_response, fail_program, fail_stop, busy_forever;
static unsigned crc_left, busy_bytes, stop_tokens;
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
  if (crc_left) {
    crc_left--;
    return 0xff;
  }
  if (data_response) {
    data_response = false;
    programming = true;
    busy_bytes = 2;
    busy_forever = fail_program;
    return 0x05;
  }
  if (programming) {
    if (busy_forever || busy_bytes > 0) {
      if (busy_bytes) {
        busy_bytes--;
      }
      return 0;
    }
    programming = false;
  }
  if (writing && byte == 0xfd) {
    stop_tokens++;
    programming = true;
    busy_bytes = 2;
    busy_forever = fail_stop;
    return 0xff;
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
        case CMD24:
        case CMD25:
          response[0] = 0;
          writing = true;
          break;
        case CMD17:
          response[0] = 0;
          response[1] = 0xfe;
          response_len = 2;
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
bool TM_SPI_ReadMulti(SPI_TypeDef *spi, uint8_t *buffer, uint8_t dummy, uint32_t count) {
  (void)spi;
  memset(buffer, dummy, count);
  read_count++;
  return transfer_ok;
}
bool TM_SPI_WriteMulti(SPI_TypeDef *spi, uint8_t *buffer, uint32_t count) {
  (void)spi;
  (void)buffer;
  (void)count;
  write_count++;
  if (writing && transfer_ok) {
    crc_left = 2;
    data_response = true;
  }
  return transfer_ok;
}

static void reset(Time start) {
  now = started = start;
  spi_step = 100;
  selected = force_busy = respond = false;
  transfer_ok = true;
  read_count = write_count = 0;
  writing = programming = data_response = fail_program = fail_stop = busy_forever = false;
  crc_left = busy_bytes = stop_tokens = 0;
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

static void test_transfer_errors(void) {
  BYTE block[512];
  for (unsigned ok = 0; ok <= 1; ok++) {
    reset(0);
    selected = true;
    transfer_ok = ok;
    response[0] = 0xfe;
    response_len = 1;
    assert(rcvr_datablock(block, sizeof(block)) == (int)ok);
    assert(read_count == 1);

    reset(0);
    selected = true;
    transfer_ok = ok;
    // Ready, token, two CRC bytes, then accepted data response.
    const BYTE reply[] = {0xff, 0xff, 0xff, 0xff, 0x05};
    memcpy(response, reply, sizeof(reply));
    response_len = sizeof(reply);
    assert(xmit_datablock(block, 0xfe) == (int)ok);
    assert(write_count == 1);

    reset(0);
    respond = true;
    transfer_ok = ok;
    TM_FATFS_SD_Stat = 0;
    TM_FATFS_SD_CardType = CT_SD2 | CT_BLOCK;
    assert(TM_FATFS_SD_disk_read(block, 0, 1) == (ok ? RES_OK : RES_ERROR));
    assert(read_count == 1);
    assert(TM_FATFS_SD_disk_write(block, 0, 1) == (ok ? RES_OK : RES_ERROR));
    assert(write_count == 1);
  }
}

static void test_write_completion(Time start, unsigned count, bool program_timeout,
                                  bool stop_timeout) {
  reset(start);
  respond = true;
  fail_program = program_timeout;
  fail_stop = stop_timeout;
  TM_FATFS_SD_Stat = 0;
  TM_FATFS_SD_CardType = CT_SD2 | CT_BLOCK;
  BYTE blocks[1024] = {0};
  DRESULT result = TM_FATFS_SD_disk_write(blocks, 0, count);
  assert(!selected);
  if (program_timeout || stop_timeout) {
    assert(result == RES_ERROR && now - started >= time_msec(5000));
  } else {
    assert(result == RES_OK && now - started < time_msec(100));
  }
  assert(stop_tokens == (count > 1 && !program_timeout ? 1U : 0U));
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
    test_write_completion(starts[i], 1, false, false);
    test_write_completion(starts[i], 2, false, false);
    test_write_completion(starts[i], 1, true, false);
    test_write_completion(starts[i], 2, true, false);
    test_write_completion(starts[i], 2, false, true);
  }
  test_transfer_errors();
  puts("sd protocol: local deadlines, late polls, rollover, and card initialization passed");
  puts("sd protocol: transfer errors propagate from SPI reads and writes");
  puts("sd protocol: single/multiple writes report programming and stop-token timeouts");
}
