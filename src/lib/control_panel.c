#include "control_panel.h"

//////////////////////////////////////////////////////////////////////////////

void init_cc_remote_calc(CCRemoteCalc *ccrc, Network *network)
{
	init_remote_keyboard_send(&ccrc->rks, network, ROCKET1_ADDR, REMOTE_SUBFOCUS_PORT0);
	init_remote_uie(&ccrc->ruie, &ccrc->rks.forwardLocalStrokes);
	ccrc->uie_handler = (UIEventHandler*) &ccrc->ruie;
	ccrc->name = "Computer";
}

//////////////////////////////////////////////////////////////////////////////

void init_cc_launch(CCLaunch *ccl, Screen4 *s4, Booster *booster, AudioClient *audioClient, ScreenBlanker *screenblanker)
{
	launch_init(&ccl->launch, s4, booster, audioClient, screenblanker);
	ccl->uie_handler = (UIEventHandler*) &ccl->launch;
	ccl->name = "Launch";
}

//////////////////////////////////////////////////////////////////////////////

void init_cc_dock(CCDock *ccd, Screen4 *s4, uint8_t auxboard0, AudioClient *audioClient, Booster *booster, JoystickState_t *joystick)
{
	ddock_init(&ccd->dock, s4, auxboard0, audioClient, booster, joystick);
	ccd->uie_handler = (UIEventHandler*) &ccd->dock.handler;
	ccd->name = "dock";
}

//////////////////////////////////////////////////////////////////////////////

void init_cc_pong(CCPong *ccp, Screen4 *s4, AudioClient *audioClient)
{
	pong_init(&ccp->pong, s4, audioClient);
	ccp->uie_handler = (UIEventHandler*) &ccp->pong.handler;
	ccp->name = "Pong";
}

//////////////////////////////////////////////////////////////////////////////

void init_cc_disco(CCDisco *ccp, AudioClient *audioClient, ScreenBlanker *screenblanker)
{
	disco_init(&ccp->disco, audioClient, screenblanker);
	ccp->uie_handler = (UIEventHandler*) &ccp->disco.handler;
	ccp->name = "disco";
}

//////////////////////////////////////////////////////////////////////////////

UIEventDisposition cp_uie_handler(ControlPanel *cp, UIEvent evt);
void cp_inject(struct s_direct_injector *di, char k);
void cp_paint(ControlPanel *cp);

void init_control_panel(ControlPanel *cp, uint8_t board0, uint8_t aux_board0, Network *network, HPAM *hpam, AudioClient *audioClient, IdleAct *idle, ScreenBlanker *screenblanker, JoystickState_t *joystick)
{
	cp->handler_func = (UIEventHandlerFunc) cp_uie_handler;

	init_screen4(&cp->s4, board0);

	cp->direct_injector.injector_func = (InputInjectorFunc) cp_inject;
	cp->direct_injector.cp = cp;

	booster_init(&cp->booster, hpam, audioClient, screenblanker);

	cp->child_count = 0;

	cp->children[cp->child_count++] = (ControlChild*) &cp->ccrc;
	init_cc_remote_calc(&cp->ccrc, network);

	cp->children[cp->child_count++] = (ControlChild*) &cp->ccl;
	init_cc_launch(&cp->ccl, &cp->s4, &cp->booster, audioClient, screenblanker);

	cp->children[cp->child_count++] = (ControlChild*) &cp->ccdock;
	init_cc_dock(&cp->ccdock, &cp->s4, aux_board0, audioClient, &cp->booster, joystick);

	cp->children[cp->child_count++] = (ControlChild*) &cp->ccpong;
	init_cc_pong(&cp->ccpong, &cp->s4, audioClient);

	cp->children[cp->child_count++] = (ControlChild*) &cp->ccdisco;
	init_cc_disco(&cp->ccdisco, audioClient, screenblanker);

	assert(cp->child_count <= CONTROL_PANEL_NUM_CHILDREN);

	cp->selected_child = 0;
	cp->active_child = CP_NO_CHILD;

	cp->idle = idle;
	s4_show(&cp->s4);

	cp_paint(cp);
}

void cp_inject(struct s_direct_injector *di, char k)
{
	di->cp->handler_func((UIEventHandler*) (di->cp), k);
}

UIEventDisposition cp_uie_handler(ControlPanel *cp, UIEvent evt)
{
	UIEventDisposition result = uied_accepted;
	if (cp->active_child != CP_NO_CHILD)
	{
		ControlChild *cc = cp->children[cp->active_child];
		if (evt==evt_remote_escape)
		{
			result = uied_blur;
		}
		else
		{
			result = cc->uie_handler->func(cc->uie_handler, evt);
		}
		if (result==uied_blur)
		{
			cc->uie_handler->func(cc->uie_handler, uie_escape);
			cp->active_child = CP_NO_CHILD;
			s4_show(&cp->s4);
		}
	}
	else
	{
		switch (evt)
		{
			case uie_right:
			{
				cp->selected_child = (cp->selected_child+1) % cp->child_count;
				break;
			}
			case uie_left:
			{
				cp->selected_child = (cp->selected_child+cp->child_count-1) % cp->child_count;
				break;
			}
			case uie_select:
			{
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

void cp_paint(ControlPanel *cp)
{
	if (cp->active_child != CP_NO_CHILD)
	{
		return;
	}

	int coff;
// weird. Calling raster_{clear|draw}_buffers blows out the register file
// and bloats the program by 56 bytes or so. I wonder if we're missing a
// -Ospace sort of argument.
//	raster_clear_buffers(&cp->s4.rrect);
	for (coff=0; coff<CONTROL_PANEL_HEIGHT; coff++)
	{
		int ci = (cp->selected_child + coff) % cp->child_count;
		ControlChild *cc = cp->children[ci];
		memset(cp->s4.bbuf[coff].buffer, 0, cp->s4.rrect.xlen);
		ascii_to_bitmap_str(cp->s4.bbuf[coff].buffer, cp->s4.rrect.xlen, cc->name);
		board_buffer_draw(&cp->s4.bbuf[coff]);
	}
//	raster_draw_buffers(&cp->s4.rrect);
}
