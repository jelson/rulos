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

#include <inttypes.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>

#include "core/board_defs.h"
#include "core/clock.h"
#include "core/cpumon.h"
#include "core/hal.h"
#include "core/network.h"
#include "core/util.h"
#include "periph/7seg_panel/7seg_panel.h"
#include "periph/audio/audio_client.h"
#include "periph/input_controller/input_controller.h"
#include "periph/rasters/rasters.h"
#include "periph/rocket/snakegame.h"
#include "periph/uart/uart.h"

/****************************************************************************/

#if !defined(JOYSTICK_X_CHAN) || !defined(JOYSTICK_Y_CHAN)
#error "JOYSTICK_X_CHAN and JOYSTICK_Y_CHAN must be defined."
#include <stophere>
#endif

#define POTSTICKER_CHANNEL 0
#define VOLUME_POT_CHANNEL 1
#define USE_LOCAL_KEYPAD 0

HalUart uart;
CpumonAct cpumon;
Snake snake;

typedef struct s_direct_injector {
  InputInjectorFunc injector_func;
} direct_injector_t;

void cp_inject(struct s_direct_injector *di, char k) {
  snake.handler.func((UIEventHandler *)(&snake.handler), k);
}

int main() {
  hal_init();

  hal_uart_init(&uart, 38400, true, /* uart_id= */ 0);
  LOG("Log output running");

#if NUM_LOCAL_BOARDS > 0
  hal_init_rocketpanel();
#endif

  // start the jiffy clock
  init_clock(10000, TIMER1);

  // includes slow calibration phase
  cpumon_init(&cpumon);

  board_buffer_module_init();

  Network network;
  init_twi_network(&network, 200, ROCKET_ADDR);

  AudioClient audio_client;
  init_audio_client(&audio_client, &network);

  Screen4 s4;
  uint8_t board0 = 3;
  init_screen4(&s4, board0);

  direct_injector_t injector;
  injector.injector_func = (InputInjectorFunc)cp_inject;
  InputPollerAct ip;
  input_poller_init(
      &ip,
      (InputInjectorIfc *)&injector);  // TODO pass keystrokes straight to snake
  snake_init(&snake, &s4, &audio_client, 2, 1);

  snake.handler.func((UIEventHandler *)&snake.handler, uie_focus);  // EWW
  cpumon_main_loop();

  return 0;
}
