#include "control_panel.h"

//////////////////////////////////////////////////////////////////////////////

void init_cc_remote_calc(CCRemoteCalc *ccrc, Network *network)
{
	init_remote_keyboard_send(&ccrc->rks, network, REMOTE_SUBFOCUS_PORT0);
	init_remote_uie(&ccrc->ruie, &ccrc->rks.forwardLocalStrokes);
	ccrc->uie_handler = (UIEventHandler*) &ccrc->ruie;
	ccrc->name = "Computer";
}

//////////////////////////////////////////////////////////////////////////////

void init_cc_launch(CCLaunch *ccl, uint8_t board0)
{
	launch_init(&ccl->launch, board0);
	ccl->uie_handler = (UIEventHandler*) &ccl->launch;
	ccl->name = "Launch";
}

//////////////////////////////////////////////////////////////////////////////

void init_cc_dock(CCDock *ccd, uint8_t board0, uint8_t auxboard0)
{
	ddock_init(&ccd->dock, board0, auxboard0, NULL);
	ccd->uie_handler = (UIEventHandler*) &ccd->dock.handler;
	ccd->name = "Dock";
}

//////////////////////////////////////////////////////////////////////////////

void init_cc_pong(CCPong *ccp, uint8_t board0)
{
	pong_init(&ccp->pong, board0, NULL);
	ccp->uie_handler = (UIEventHandler*) &ccp->pong.handler;
	ccp->name = "Pong";
}

//////////////////////////////////////////////////////////////////////////////

UIEventDisposition cp_uie_handler(ControlPanel *cp, UIEvent evt);
void cp_inject(struct s_direct_injector *di, char k);
void cp_paint(ControlPanel *cp);

void init_control_panel(ControlPanel *cp, uint8_t board0, uint8_t aux_board0, Network *network)
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

	cp->children[cp->child_count++] = (ControlChild*) &cp->ccdock;
	init_cc_dock(&cp->ccdock, board0, aux_board0);

	cp->children[cp->child_count++] = (ControlChild*) &cp->ccpong;
	init_cc_pong(&cp->ccpong, board0);

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
			cc->uie_handler->func(cc->uie_handler, uie_escape);
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
				cc->uie_handler->func(cc->uie_handler, uie_focus);
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
