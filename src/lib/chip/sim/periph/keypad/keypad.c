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

#include "chip/sim/core/sim.h"
#include "core/keystrokes.h"
#include "core/logging.h"
#include "core/queue.h"
#include "core/util.h"

/************ keypad simulator *********************/

r_bool sim_keypad_keystroke_handler(char c);
char translate_to_keybuf(char c);

r_bool g_keypad_enabled = FALSE;
char keypad_buf[10];
CharQueue *keypad_q = (CharQueue *)keypad_buf;

void hal_init_keypad() {
  g_keypad_enabled = TRUE;
  CharQueue_init((CharQueue *)keypad_buf, sizeof(keypad_buf));

  sim_maybe_init_and_register_keystroke_handler(sim_keypad_keystroke_handler);
}

r_bool sim_keypad_keystroke_handler(char c) {
  char k;
  if ((k = translate_to_keybuf(c)) != 0) {
    CharQueue_append(keypad_q, k);
    return true;
  }
  return false;
}

char hal_read_keybuf() {
  if (!g_keypad_enabled) {
    return 0;
  }

  char k;

  if (CharQueue_pop(keypad_q, &k))
    return k;
  else
    return 0;
}

char hal_scan_keypad() { return 0; }

// translation from a key typed at the keyboard to the simulated
// keypad input that should be enqueued
char translate_to_keybuf(char c) {
  if (c >= 'a' && c <= 'd') return c;
  if (c == '\t') return 'a';
  if (c == '\n') return 'c';
  if (c == 27) return 'd';

  if (c >= '0' && c <= '9') return c;

  if (c == '*' || c == 's' || c == '.') return 's';

  if (c == 'p' || c == '#') return 'p';

  return 0;
}
