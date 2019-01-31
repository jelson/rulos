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

/*
 * hardware.c: These functions are only needed for physical display hardware.
 *
 * This file is not compiled by the simulator.
 */

#include <stdarg.h>
#include <stddef.h>
#include <stdio.h>

#include "core/hal.h"
#include "core/logging.h"

//////////////////////////////////////////////////////////////////////////////

HalUart* g_uart_handler[2] = {NULL, NULL};

void hal_uart_init(HalUart* handler, uint32_t baud, r_bool stop2,
                   uint8_t uart_id) {
  handler->uart_id = uart_id;

  // TODO: Init actual serial hardware
}

void uart_sync_send_by_id(const uint8_t uart_id, const char* s) {
  // TODO: send
}

void uart_flush(const uint8_t uart_id) {
  // TODO: flush
}

void hal_uart_sync_send(HalUart* handler, const char* s) {
  uart_sync_send_by_id(handler->uart_id, s);
}

#ifdef LOG_TO_SERIAL

// This silliness is because "gcc -DLOG_TO_SERIAL" defaults to
// defining LOG_TO_SERIAL to be 1, which is a confusing default.
// I want to force you to explicitly name either UART0 or UART1
// as your desired logging destination.

#define UART0 100
#define UART1 200

#if LOG_TO_SERIAL == UART0
#define LOGGING_UART 0
#elif LOG_TO_SERIAL == UART1
#define LOGGING_UART 1
#else
#error LOG_TO_SERIAL must be set to UART0 or UART1
#include <stophere>
#endif

#undef UART0
#undef UART1

void arm_log(const char* fmt, ...) {
  va_list ap;
  char message[100];
  va_start(ap, fmt);
  vsnprintf(message, sizeof(message), fmt, ap);
  va_end(ap);
  uart_sync_send_by_id(LOGGING_UART, message);
}

void arm_log_flush() { uart_flush(LOGGING_UART); }

#endif  // LOG_TO_SERIAL
