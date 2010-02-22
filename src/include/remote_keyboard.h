#ifndef _remote_keyboard_h
#define _remote_keyboard_h

#include "rocket.h"
#include "network.h"
#include "network_ports.h"
#include "input_controller.h"

typedef struct {
	char key;
} KeystrokeMessage;

typedef struct s_remote_keyboard_send {
	Network *network;
	Port port;
	uint8_t send_msg_alloc[sizeof(Message)+sizeof(KeystrokeMessage)];
	SendSlot sendSlot;

	InputInjectorIfc forwardLocalStrokes;
	struct s_remote_keyboard_send *forward_this;
} RemoteKeyboardSend;

void init_remote_keyboard_send(RemoteKeyboardSend *rk, Network *network, Addr addr, Port port);

typedef struct s_remote_keyboard_recv {
	uint8_t recv_msg_alloc[sizeof(Message)+sizeof(KeystrokeMessage)];
	RecvSlot recvSlot;
	InputInjectorIfc *acceptNetStrokes;
} RemoteKeyboardRecv;

void init_remote_keyboard_recv(RemoteKeyboardRecv *rk, Network *network, InputInjectorIfc *acceptNetStrokes, Port port);

#endif // _remote_keyboard_h
