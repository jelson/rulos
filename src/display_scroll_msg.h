#ifndef display_scroll_msg_h
#define display_scroll_msg_h

#include "clock.h"

typedef struct s_dscrollmsgact {
	ActivationFunc func;
	uint8_t board;
	uint8_t len;
	uint8_t speed_ms;
	int index;
	char *msg;
} DScrollMsgAct;


void dscrlmsg_init(struct s_dscrollmsgact *act,
	uint8_t board, char *msg, uint8_t speed_ms);

#endif // display_scroll_msg_h
