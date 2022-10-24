//// serial reader

#include "serial_reader.h"

#include <stdio.h>
#include <string.h>

#include "core/clock.h"
#include "flash_dumper.h"

static void sr_line_received(UartState_t *uart, void *user_data, char *line) {
  serial_reader_t *sr = (serial_reader_t *)user_data;
  sr->last_active = clock_time_us();
  flash_dumper_write(sr->flash_dumper, line, strlen(line), "in,%d,%d,",
                     uart->uart_id, sr->num_total_lines++);

  if (sr->cb != NULL) {
    sr->cb(sr, line);
  }
}

void serial_reader_init(serial_reader_t *sr, uint8_t uart_id, uint32_t baud,
                        flash_dumper_t *flash_dumper, serial_reader_cb_t cb) {
  memset(sr, 0, sizeof(*sr));
  sr->last_active = clock_time_us();
  sr->flash_dumper = flash_dumper;
  sr->cb = cb;
  uart_init(&sr->uart, uart_id, baud);
  linereader_init(&sr->linereader, &sr->uart, sr_line_received, sr);
}

void serial_reader_print(serial_reader_t *sr, const char *s) {
  uart_print(&sr->uart, s);

  // record the fact that we sent this string to the uart
  flash_dumper_write(sr->flash_dumper, s, strlen(s), "out,%d",
                     sr->uart.uart_id);
}

#define ACTIVITY_TIMEOUT_US 2000000

bool serial_reader_is_active(serial_reader_t *sr) {
  Time now = clock_time_us();
  Time time_since_active = now - sr->last_active;

  if (time_since_active >= ACTIVITY_TIMEOUT_US) {
    // if inactive, move the last-active time forward to prevent rollover
    // problems (but still far enough in the past that the serial reader does
    // not appear to be artificially active)
    sr->last_active = now - ACTIVITY_TIMEOUT_US - 1;
    return false;
  } else {
    return true;
  }
}
