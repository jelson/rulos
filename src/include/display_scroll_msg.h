#ifndef __display_scroll_msg_h__
#define __display_scroll_msg_h__

#include "clock.h"
#include "board_buffer.h"

typedef struct s_dscrollmsgact {
	ActivationFunc func;
	BoardBuffer bbuf;
	uint8_t len;
	uint8_t speed_ms;
	int index;
	char *msg;
} DScrollMsgAct;


void dscrlmsg_init(struct s_dscrollmsgact *act,
	uint8_t board, char *msg, uint8_t speed_ms);

void dscrlmsg_set_msg(DScrollMsgAct *act, char *msg);

#endif // display_scroll_msg_h