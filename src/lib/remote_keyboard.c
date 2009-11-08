#include "remote_keyboard.h"

void rk_send(InputInjectorIfc *injector, char key);
void rk_send_complete(SendSlot *sendSlot);
void rk_recv(RecvSlot *recvSlot);

void init_remote_keyboard_send(RemoteKeyboardSend *rk, Network *network)
{
	rk->network = network;

	rk->sendSlot.func = rk_send_complete;
	rk->sendSlot.msg = (Message*) rk->send_msg_alloc;
	rk->sendSlot.sending = FALSE;

	rk->forwardLocalStrokes.func = rk_send;
	rk->forward_this = rk;
}

void rk_send(InputInjectorIfc *injector, char key)
{
	RemoteKeyboardSend *rk = *(RemoteKeyboardSend **) (injector+1);

	if (rk->sendSlot.sending)
	{
		LOGF((logfp, "RemoteKeyboard drops a message due to full send queue.\n"));
		return;
	}
	rk->sendSlot.msg->dest_port = REMOTE_KEYBOARD_PORT;
	rk->sendSlot.msg->message_size = sizeof(KeystrokeMessage);
	KeystrokeMessage *km = (KeystrokeMessage *) &rk->sendSlot.msg->data;
	km->key = key;
	net_send_message(rk->network, &rk->sendSlot);
}

void rk_send_complete(SendSlot *sendSlot)
{
	sendSlot->sending = FALSE;
}

void init_remote_keyboard_recv(RemoteKeyboardRecv *rk, Network *network, InputInjectorIfc *acceptNetStrokes)
{
	rk->recvSlot.func = rk_recv;
	rk->recvSlot.port = REMOTE_KEYBOARD_PORT;
	rk->recvSlot.payload_capacity = sizeof(KeystrokeMessage);
	rk->recvSlot.msg_occupied = FALSE;
	rk->recvSlot.msg = (Message*) rk->recv_msg_alloc;
	rk->recv_this = rk;

	rk->acceptNetStrokes = acceptNetStrokes;

	net_bind_receiver(network, &rk->recvSlot);
}

void rk_recv(RecvSlot *recvSlot)
{
	RemoteKeyboardRecv *rk = *(RemoteKeyboardRecv **)(recvSlot+1);
	KeystrokeMessage *km = (KeystrokeMessage *) &recvSlot->msg->data;
	rk->acceptNetStrokes->func(rk->acceptNetStrokes, km->key);
	recvSlot->msg_occupied = FALSE;
}
