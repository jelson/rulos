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

#include <ctype.h>
#include <curses.h>
#include <fcntl.h>
#include <sched.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/timeb.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>

#include "core/curses.h"
#include "core/keystrokes.h"
#include "core/logging.h"
#include "core/sim.h"

static void sim_curses_poll(void *data);

static sim_special_input_handler_t sim_special_input_handler = NULL;
static sim_input_handler_stop_t sim_input_handler_stop = NULL;
static r_bool initted = false;

#define MAX_HANDLERS 10
static int keystroke_handler_count;
static sim_keystroke_handler keystroke_handlers[MAX_HANDLERS];

void sim_maybe_init_and_register_keystroke_handler(
    sim_keystroke_handler handler) {
  if (!initted) {
    sim_register_clock_handler(sim_curses_poll, NULL);
    init_curses();  // Need curses to receive input.
    initted = true;
  }
  keystroke_handlers[keystroke_handler_count++] = handler;
  assert(keystroke_handler_count <= MAX_HANDLERS);
}

void sim_install_modal_handler(sim_special_input_handler_t handler,
                               sim_input_handler_stop_t stop) {
  sim_special_input_handler = handler;
  sim_input_handler_stop = stop;
}

static void cleanup_special_handler() {
  sim_input_handler_stop();
  sim_special_input_handler = NULL;
  sim_input_handler_stop = NULL;
  touchwin(curses_get_window());
  refresh();
}

static void terminate_sim(void) {
  nocbreak();
  endwin();
  exit(0);
}

static void sim_curses_poll(void *data) {
  int c = getch();

  if (c == ERR) return;

  LOG("poll_kb got char: %c (%x)\n", c, c);

  // if we're in normal mode and hit 'q', terminate the simulator
  if (sim_special_input_handler == NULL && c == 'q') {
    terminate_sim();
  }

  // If we're in a special input mode and hit escape, exit the mode.
  // Otherwise, pass the character to the handler.
  if (sim_special_input_handler != NULL) {
    if (c == 27 /* escape */) {
      cleanup_special_handler();
    } else {
      sim_special_input_handler(c);
    }
    return;
  }

  for (int i = 0; i < keystroke_handler_count; i++) {
    sim_keystroke_handler handler = keystroke_handlers[i];
    if (handler(c)) {
      return;
    }
  }
  LOG("** No handler for: %c (%x)\n", c, c);
}