#ifndef display_scroll_msg_h
#define display_scroll_msg_h

#include "clock.h"

typedef struct s_dscrollmsgact {
	ActivationFunc activation;
	int board;
	char *msg;
	uint8_t len;
	uint8_t speed_ms;
} DScrollMsgAct;


void dscrlmsg_init(struct s_dscrollmsgact *act,
	uint8_t board, char *msg, uint8_t speed_ms);

#endif // display_scroll_msg_h
