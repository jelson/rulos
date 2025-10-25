/*
 * Copyright (C) 2009 Jon Howell (jonh@jonh.net) and Jeremy Elson
 * (jelson@gmail.com).
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */

#include "periph/rocket/control_panel.h"

//////////////////////////////////////////////////////////////////////////////

void init_cc_local_calc(CCLocalCalc *cclc,
                        FetchCalcDecorationValuesIfc *decoration_ifc) {
  calculator_init(&cclc->calc, 12 /*and 13*/, NULL, decoration_ifc);
  cclc->uie_handler = (UIEventHandler *)&cclc->calc.focus;
  cclc->name = "Computer";
}

//////////////////////////////////////////////////////////////////////////////

void init_cc_remote_calc(CCRemoteCalc *ccrc, Network *network) {
  init_remote_keyboard_send(&ccrc->rks, network, ROCKET1_ADDR,
                            REMOTE_SUBFOCUS_PORT0);
  init_remote_uie(&ccrc->ruie, &ccrc->rks.forwardLocalStrokes);
  ccrc->uie_handler = (UIEventHandler *)&ccrc->ruie;
  ccrc->name = "Computer";
}

//////////////////////////////////////////////////////////////////////////////

void init_cc_launch(CCLaunch *ccl, Screen4 *s4, Booster *booster, HPAM *hpam,
                    ThrusterState_t *thrusterState, AudioClient *audioClient,
                    ScreenBlanker *screenblanker) {
  launch_init(&ccl->launch, s4, booster, hpam, thrusterState, audioClient,
              screenblanker);
  ccl->uie_handler = (UIEventHandler *)&ccl->launch;
  ccl->name = "Launch";
}

//////////////////////////////////////////////////////////////////////////////

void init_cc_dock(CCDock *ccd, Screen4 *s4, uint8_t auxboard0,
                  AudioClient *audioClient, Booster *booster,
                  JoystickState_t *joystick) {
  ddock_init(&ccd->dock, s4, auxboard0, audioClient, booster, joystick);
  ccd->uie_handler = (UIEventHandler *)&ccd->dock.handler;
  ccd->name = "dock";
}

//////////////////////////////////////////////////////////////////////////////

void init_cc_pong(CCPong *ccp, Screen4 *s4, AudioClient *audioClient) {
  pong_init(&ccp->pong, s4, audioClient);
  ccp->uie_handler = (UIEventHandler *)&ccp->pong.handler;
  ccp->name = "Pong";
}

//////////////////////////////////////////////////////////////////////////////
//
void init_cc_snake(CCSnake *ccs, Screen4 *s4, AudioClient *audioClient) {
  snake_init(&ccs->snake, s4, audioClient, 1, 2);
  ccs->uie_handler = (UIEventHandler *)&ccs->snake.handler;
  ccs->name = "Snake";
}

//////////////////////////////////////////////////////////////////////////////

void init_cc_invaders(CCInvaders *cci, Screen4 *s4, AudioClient *audioClient,
                      JoystickState_t *joystick) {
  invaders_init(&cci->invaders, s4, audioClient, joystick, 1, 2);
  cci->uie_handler = (UIEventHandler *)&cci->invaders.handler;
  cci->name = "Invaders";
}

//////////////////////////////////////////////////////////////////////////////

void init_cc_disco(CCDisco *ccp, AudioClient *audioClient,
                   ScreenBlanker *screenblanker, IdleAct *idle, Network* network) {
  disco_init(&ccp->disco, audioClient, screenblanker, idle, network);
  ccp->uie_handler = (UIEventHandler *)&ccp->disco.handler;
  ccp->name = "disco";
}

//////////////////////////////////////////////////////////////////////////////

UIEventDisposition cp_uie_handler(ControlPanel *cp, UIEvent evt);
void cp_inject(struct s_direct_injector *di, char k);
void cp_paint(ControlPanel *cp);

void init_control_panel(ControlPanel *cp, uint8_t board0, uint8_t aux_board0,
                        Network *network, HPAM *hpam, AudioClient *audioClient,
                        IdleAct *idle, ScreenBlanker *screenblanker,
                        JoystickState_t *joystick,
                        ThrusterState_t *thrusterState, Keystroke vol_up_key,
                        Keystroke vol_down_key,
                        FetchCalcDecorationValuesIfc *decoration_ifc) {
  cp->handler_func = (UIEventHandlerFunc)cp_uie_handler;

  init_screen4(&cp->s4, board0);

  cp->direct_injector.injector_func = (InputInjectorFunc)cp_inject;
  cp->direct_injector.cp = cp;

  booster_init(&cp->booster, hpam, audioClient, screenblanker);

  cp->child_count = 0;

#if defined(BOARDCONFIG_ROCKET0) || defined(BOARDCONFIG_NETROCKET)
  cp->children[cp->child_count++] = (ControlChild *)&cp->ccrc;
  init_cc_remote_calc(&cp->ccrc, network);
#else
  cp->children[cp->child_count++] = (ControlChild *)&cp->cclc;
  init_cc_local_calc(&cp->cclc, decoration_ifc);
#endif

  cp->children[cp->child_count++] = (ControlChild *)&cp->ccl;
  init_cc_launch(&cp->ccl, &cp->s4, &cp->booster, hpam, thrusterState,
                 audioClient, screenblanker);

  cp->children[cp->child_count++] = (ControlChild *)&cp->ccdock;
  init_cc_dock(&cp->ccdock, &cp->s4, aux_board0, audioClient, &cp->booster,
               joystick);

  cp->children[cp->child_count++] = (ControlChild *)&cp->ccpong;
  init_cc_pong(&cp->ccpong, &cp->s4, audioClient);

  cp->children[cp->child_count++] = (ControlChild *)&cp->ccsnake;
  init_cc_snake(&cp->ccsnake, &cp->s4, audioClient);

  cp->children[cp->child_count++] = (ControlChild *)&cp->ccinvaders;
  init_cc_invaders(&cp->ccinvaders, &cp->s4, audioClient, joystick);

  cp->children[cp->child_count++] = (ControlChild *)&cp->ccdisco;
  init_cc_disco(&cp->ccdisco, audioClient, screenblanker, idle, network);

  assert(cp->child_count <= CONTROL_PANEL_NUM_CHILDREN);

  cp->vol_up_key = vol_up_key;
  cp->vol_down_key = vol_down_key;
  volume_control_init(&cp->volume_control, audioClient,
                      /*board*/ 0, vol_up_key, vol_down_key);

  cp->selected_child = 0;
  cp->active_child = CP_NO_CHILD;

  cp->idle = idle;
  s4_show(&cp->s4);

  cp_paint(cp);
}

void cp_inject(struct s_direct_injector *di, char k) {
  di->cp->handler_func((UIEventHandler *)(di->cp), k);
}

UIEventDisposition cp_uie_handler(ControlPanel *cp, UIEvent evt) {
  UIEventDisposition result = uied_accepted;

  if (KeystrokeCmp(KeystrokeCtor(evt), cp->vol_up_key)
      || KeystrokeCmp(KeystrokeCtor(evt), cp->vol_down_key)) {
    // steal these events for volume control
    (cp->volume_control.injector.iii.func)(&cp->volume_control.injector.iii, KeystrokeCtor(evt));
    return result;
  }

  if (cp->active_child != CP_NO_CHILD) {
    ControlChild *cc = cp->children[cp->active_child];
    if (evt == evt_remote_escape) {
      result = uied_blur;
    } else {
      result = cc->uie_handler->func(cc->uie_handler, evt);
    }
    if (result == uied_blur) {
      cc->uie_handler->func(cc->uie_handler, uie_escape);
      cp->active_child = CP_NO_CHILD;
      s4_show(&cp->s4);
    }
  } else {
    switch (evt) {
      case uie_right: {
        cp->selected_child = (cp->selected_child + 1) % cp->child_count;
        break;
      }
      case uie_left: {
        cp->selected_child =
            (cp->selected_child + cp->child_count - 1) % cp->child_count;
        break;
      }
      case uie_select: {
        cp->active_child = cp->selected_child;
        ControlChild *cc = cp->children[cp->active_child];
        cc->uie_handler->func(cc->uie_handler, uie_focus);
      }
    }
  }
  cp_paint(cp);
  idle_touch(cp->idle);
  return result;
}

void cp_paint(ControlPanel *cp) {
  if (cp->active_child != CP_NO_CHILD) {
    return;
  }

  // weird. Calling raster_{clear|draw}_buffers blows out the register file
  // and bloats the program by 56 bytes or so. I wonder if we're missing a
  // -Ospace sort of argument.
  //	raster_clear_buffers(&cp->s4.rrect);
  for (int coff = 0; coff < CONTROL_PANEL_HEIGHT; coff++) {
    int ci = (cp->selected_child + coff) % cp->child_count;
    ControlChild *cc = cp->children[ci];
    memset(cp->s4.bbuf[coff].buffer, 0, cp->s4.rrect.xlen);
    ascii_to_bitmap_str(cp->s4.bbuf[coff].buffer, cp->s4.rrect.xlen, cc->name);
    board_buffer_draw(&cp->s4.bbuf[coff]);
  }
  //	raster_draw_buffers(&cp->s4.rrect);
}
