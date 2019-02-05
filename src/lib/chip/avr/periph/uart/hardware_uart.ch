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

// This file contains functions statically parameterized by UART compiler
// definitions; we include it multiple times to provide access to both
// UARTs.

void hal_uart_init_name(HalUart *handler, uint32_t baud, r_bool stop2,
                        uint8_t uart_id) {
  uint16_t ubrr = baud_to_ubrr(baud);

  // disable interrupts
  cli();

  // set up baud rate
  _UBRRH = (unsigned char)(ubrr >> 8);
  _UBRRL = (unsigned char)ubrr;

  _UCSRB = _BV(_TXEN) |  // enable transmitter
           _BV(_RXEN)    // enable receiver
      ;

  if (handler != NULL) {
    _UCSRB |= _BV(_RXCIE);  // enable receiver interrupt
  }

  // set frame format: async, 8 bit data, 1 stop bit, no parity
  _UCSRC = _BV(_UCSZ1) | _BV(_UCSZ0) | (stop2 ? _BV(_USBS) : 0)
#ifdef MCU8_line
           | _BV(URSEL)
#endif
      ;

  g_uart_handler[uart_id] = handler;

  // enable interrupts, whether or not they'd been previously enabled
  sei();
}

static inline void enable_sendready_interrupt_name(uint8_t enable) {
  if (enable) {
    reg_set(&_UCSRB, _UDRIE);
  } else {
    reg_clr(&_UCSRB, _UDRIE);
  }
}

// Runs in user context.
void hal_uart_start_send_name(void) {
  // Just enable the send-ready interrupt.  It will fire as soon as
  // the sender register is free.
  uint8_t old_interrupts = hal_start_atomic();
  enable_sendready_interrupt_name(TRUE);
  hal_end_atomic(old_interrupts);
}

// Runs in interrupt context.
static inline void _handle_recv_ready_name(char c) {
  // audioled_set(0, 1);
  if (g_uart_handler[UARTID] != NULL && g_uart_handler[UARTID]->recv != NULL) {
    (g_uart_handler[UARTID]->recv)(g_uart_handler[UARTID], c);
  }
  // audioled_set(1, 0);
}

// Runs in interrupt context.
static inline void _handle_send_ready_name() {
  char c;

  // If there is still data remaining to send, send it.  Otherwise,
  // disable the send-ready interrupt.
  if (g_uart_handler[UARTID] != NULL && g_uart_handler[UARTID]->send != NULL) {
    if ((g_uart_handler[UARTID]->send)(g_uart_handler[UARTID], &c)) {
      _UDR = c;
    } else {
      enable_sendready_interrupt_name(FALSE);
    }
  }
}

ISR(_USART_RXC_vect) { _handle_recv_ready_name(_UDR); }
ISR(_USART_UDRE_vect) { _handle_send_ready_name(); }

static inline void sync_send_byte_name(const char c) {
  while (!(_UCSRA & _BV(_UDRE))) {
    ;
  }
  _UDR = c;
}

void hal_uart_sync_send_bytes_name(const char *s, uint8_t len) {
  for (uint8_t i = 0; i < len; i++) {
    sync_send_byte_name(*s++);
  }
}

void hal_uart_sync_send_name(const char *s) {
  while (*s != '\0') {
    sync_send_byte_name(*s++);
  }
}
