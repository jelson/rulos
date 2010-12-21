#ifndef _MEDIA_H
#define _MEDIA_H

#include <stdint.h>

typedef uint8_t Addr;

struct s_MediaRecvSlot;
typedef void (*MediaRecvDoneFunc)(struct s_MediaRecvSlot *recvSlot, uint8_t len);
typedef struct s_MediaRecvSlot {
	MediaRecvDoneFunc func;
	uint8_t capacity;
	uint8_t occupied;
	void *user_data; // storage for a pointer back to your state structure
	char data[0];
} MediaRecvSlot;

typedef void (*MediaSendDoneFunc)(void *user_data);
typedef struct s_MediaStateIfc {
	void (*send)(struct s_MediaStateIfc *media,
		Addr dest_addr, char *data, uint8_t len, 
		MediaSendDoneFunc sendDoneCB, void *sendDoneCBData);
} MediaStateIfc;

#endif // _MEDIA_H
