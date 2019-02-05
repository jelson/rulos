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

#include "core/clock.h"
#include "core/network.h"
#include "core/util.h"
#include "periph/lcd_12232/graphic_lcd_12232.h"
/*
#include "periph/audio/audio_driver.h"
#include "periph/audio/audio_server.h"
#include "periph/audio/audio_streamer.h"
#include "periph/sdcard/sdcard.h"
*/
#include "periph/uart/serial_console.h"

#if !SIM
#include "core/hardware.h"
#endif  // SIM

//////////////////////////////////////////////////////////////////////////////

SerialConsole *g_console;

#define SYNCDEBUG() syncdebug(0, 'T', __LINE__)
void syncdebug(uint8_t spaces, char f, uint16_t line) {
  char buf[16], hexbuf[6];
  int i;
  for (i = 0; i < spaces; i++) {
    buf[i] = ' ';
  }
  buf[i++] = f;
  buf[i++] = ':';
  buf[i++] = '\0';
  if (f >= 'a')  // lowercase -> hex value; so sue me
  {
    debug_itoha(hexbuf, line);
  } else {
    itoda(hexbuf, line);
  }
  strcat(buf, hexbuf);
  strcat(buf, "\n");
  serial_console_sync_send(g_console, buf, strlen(buf));
}

//////////////////////////////////////////////////////////////////////////////

typedef struct {
  uint8_t val;
} BlinkAct;

void _update_blink(BlinkAct *ba) {
  ba->val += 1;
#if !SIM
  gpio_set_or_clr(GPIO_B0, ((ba->val >> 0) & 0x1) == 0);
#if 0
	gpio_set_or_clr(GPIO_B1, ((ba->val>>1)&0x1)==0);
	gpio_set_or_clr(GPIO_B2, ((ba->val>>2)&0x1)==0);
	gpio_set_or_clr(GPIO_B3, ((ba->val>>3)&0x1)==0);
#endif
  gpio_set_or_clr(GPIO_D3, ((ba->val >> 4) & 0x1) == 0);
#if 0
	gpio_set_or_clr(GPIO_D4, ((ba->val>>5)&0x1)==0);
	gpio_set_or_clr(GPIO_D5, ((ba->val>>6)&0x1)==0);
	gpio_set_or_clr(GPIO_D6, ((ba->val>>7)&0x1)==0);
	gpio_set_or_clr(GPIO_C0, ((ba->val>>0)&0x1)==0);
	gpio_set_or_clr(GPIO_C1, ((ba->val>>0)&0x1)==0);
	gpio_set_or_clr(GPIO_C2, ((ba->val>>0)&0x1)==0);
	gpio_set_or_clr(GPIO_C3, ((ba->val>>0)&0x1)==0);
	gpio_set_or_clr(GPIO_C4, ((ba->val>>0)&0x1)==0);
	gpio_set_or_clr(GPIO_C5, ((ba->val>>0)&0x1)==0);
	gpio_set_or_clr(GPIO_C6, ((ba->val>>0)&0x1)==0);
	gpio_set_or_clr(GPIO_C7, ((ba->val>>0)&0x1)==0);
#endif
#endif  // !SIM
  if ((ba->val & 0xf) == 0) {
    //		SYNCDEBUG();
  }
  schedule_us(100000, (ActivationFuncPtr)_update_blink, &ba);
}

void blink_init(BlinkAct *ba) {
  ba->val = 0;
#if !SIM
  gpio_make_output(GPIO_B0);
  gpio_make_output(GPIO_B1);
  gpio_make_output(GPIO_B2);
  gpio_make_output(GPIO_B3);
  gpio_make_output(GPIO_D3);
  gpio_make_output(GPIO_D4);
  gpio_make_output(GPIO_D5);
  gpio_make_output(GPIO_D6);
#endif  // !SIM
  schedule_us(100000, (ActivationFuncPtr)_update_blink, &ba);
}

//////////////////////////////////////////////////////////////////////////////

typedef struct {
  SerialConsole console;
  r_bool toggle;
  GLCD *glcd;
} Shell;

void shell_func(Shell *shell);

void shell_init(Shell *shell, GLCD *glcd) {
  shell->glcd = glcd;
  serial_console_init(&shell->console, (ActivationFuncPtr)shell_func, &shell);
  g_console = &shell->console;
  SYNCDEBUG();
}

void shell_func(Shell *shell) {
  char *line = shell->console.line;

  shell->toggle = !shell->toggle;
#if !SIM
  gpio_set_or_clr(GPIO_D3, shell->toggle);

  if (strncmp(line, "glyph", 5) == 0) {
    glcd_clear_framebuffer(shell->glcd);
    SYNCDEBUG();
    char *p;
    uint8_t dx0 = 0;
    for (p = &line[6]; *p != 0 && *p != '\n'; p++) {
      dx0 += glcd_paint_char(shell->glcd, *p, dx0, false);
    }
    glcd_draw_framebuffer(shell->glcd);
  } else if (strncmp(line, "vbl", 3) == 0) {
    uint8_t v = line[4] - '0';
    //		glcd_set_backlight(v!=0);
    syncdebug(2, 'b', v != 0);
  } else if (strncmp(line, "status", 6) == 0) {
    SYNCDEBUG();
    //		uint8_t bias = line[7]-'0';
    //		uint16_t v = glcd_read_status(bias);
    //		syncdebug(2, 's', v);
  } else if (strncmp(line, "display", 7) == 0) {
    uint8_t v = line[8] - '0';
    syncdebug(2, 'd', v != 0);
    //		glcd_set_display_on(v!=0);
  } else if (strncmp(line, "status", 6) == 0) {
    SYNCDEBUG();
    //		uint8_t bias = line[7]-'0';
    //		uint16_t v = glcd_read_status(bias);
    //		syncdebug(2, 's', v);
  } else if (strcmp(line, "read\n") == 0) {
    SYNCDEBUG();
    //		glcd_read_test();
  } else if (strcmp(line, "write\n") == 0) {
    SYNCDEBUG();
    //		glcd_write_test();
  }
#else
  line++;
#endif  // !SIM
  serial_console_sync_send(&shell->console, "OK\n", 3);
}

//////////////////////////////////////////////////////////////////////////////

int main() {
  hal_init();
  init_clock(1000, TIMER1);

  GLCD glcd;
  glcd_init(&glcd, NULL, NULL);

  Shell shell;
  shell_init(&shell, &glcd);

  BlinkAct ba;
  blink_init(&ba);

  CpumonAct cpumon;
  cpumon_init(&cpumon);  // includes slow calibration phase
  cpumon_main_loop();

  return 0;
}
