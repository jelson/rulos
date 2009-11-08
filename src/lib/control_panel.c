#include "control_panel.h"

//////////////////////////////////////////////////////////////////////////////

void ccrc_activate(CCRemoteCalc *ccrc)
{
	ccrc->uie_handler->func(ccrc->uie_handler, uie_focus);
}

void ccrc_deactivate(CCRemoteCalc *ccrc)
{
	// a little network-droppiness-protection: be sure guy on other
	// end has heard that he's not active.
	ccrc->uie_handler->func(ccrc->uie_handler, uie_escape);
}

void init_cc_remote_calc(CCRemoteCalc *ccrc, Network *network)
{
	ccrc->activateFunc = (CCActivateFunc) ccrc_activate;
	ccrc->deactivateFunc = (CCActivateFunc) ccrc_deactivate;
	init_remote_keyboard_send(&ccrc->rks, network, REMOTE_SUBFOCUS_PORT0);
	init_remote_uie(&ccrc->ruie, &ccrc->rks.forwardLocalStrokes);
	ccrc->uie_handler = (UIEventHandler*) &ccrc->ruie;
	ccrc->name = "Computer";
}

//////////////////////////////////////////////////////////////////////////////

void ccl_activate(CCRemoteCalc *ccl)
{
	ccl->uie_handler->func(ccl->uie_handler, uie_focus);
}

void ccl_deactivate(CCRemoteCalc *ccl)
{
	ccl->uie_handler->func(ccl->uie_handler, uie_escape);
}

void init_cc_launch(CCLaunch *ccl, uint8_t board0)
{
	ccl->activateFunc = (CCActivateFunc) ccl_activate;
	ccl->deactivateFunc = (CCActivateFunc) ccl_deactivate;
	launch_init(&ccl->launch, board0, NULL);
	ccl->uie_handler = (UIEventHandler*) &ccl->launch;
	ccl->name = "Launch";
}

//////////////////////////////////////////////////////////////////////////////

UIEventDisposition cp_uie_handler(ControlPanel *cp, UIEvent evt);
void cp_inject(struct s_direct_injector *di, char k);
void cp_paint(ControlPanel *cp);

void init_control_panel(ControlPanel *cp, uint8_t board0, Network *network)
{
	cp->handler_func = (UIEventHandlerFunc) cp_uie_handler;

	int bbi;
	for (bbi=0; bbi<CONTROL_PANEL_HEIGHT; bbi++)
	{
		cp->btable[bbi] = &cp->bbuf[bbi];
		board_buffer_init(cp->btable[bbi]);
		board_buffer_push(cp->btable[bbi], board0+bbi);
	}
	cp->rrect.bbuf = cp->btable;
	cp->rrect.ylen = CONTROL_PANEL_HEIGHT;
	cp->rrect.x = 0;
	cp->rrect.xlen = 8;

	cp->direct_injector.injector_func = (InputInjectorFunc) cp_inject;
	cp->direct_injector.cp = cp;
	
	cp->child_count = 0;

	cp->children[cp->child_count++] = (ControlChild*) &cp->ccrc;
	init_cc_remote_calc(&cp->ccrc, network);

	cp->children[cp->child_count++] = (ControlChild*) &cp->ccl;
	init_cc_launch(&cp->ccl, board0);

	cp->selected_child = 0;
	cp->active_child = CP_NO_CHILD;

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
			cc->deactivateFunc(cc);
			cp->active_child = CP_NO_CHILD;
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
				cc->activateFunc(cc);
			}
		}
	}
	cp_paint(cp);
	return result;
}

void cp_paint(ControlPanel *cp)
{
	int coff;
	for (coff=0; coff<cp->child_count; coff++)
	{
		int ci = (cp->selected_child + coff) % cp->child_count;
		ControlChild *cc = cp->children[ci];
		if (coff < CONTROL_PANEL_NUM_CHILDREN)
		{
			memset(cp->btable[coff]->buffer, 0, cp->rrect.xlen);
			ascii_to_bitmap_str(cp->btable[coff]->buffer, cp->rrect.xlen, cc->name);
			board_buffer_draw(cp->btable[coff]);
		}
	}
}
