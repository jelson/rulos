#ifndef display_scroll_msg_h
#define display_scroll_msg_h

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
	// speed_ms==0 produces non-scrolling display with no cpu overhead

void dscrlmsg_set_msg(DScrollMsgAct *act, char *msg);

#endif // display_scroll_msg_h
