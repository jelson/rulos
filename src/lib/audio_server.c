/************************************************************************
 *
 * This file is part of RulOS, Ravenna Ultra-Low-Altitude Operating
 * System -- the free, open-source operating system for microcontrollers.
 *
 * Written by Jon Howell (jonh@jonh.net) and Jeremy Elson (jelson@gmail.com),
 * May 2009.
 *
 * This operating system is in the public domain.  Copyright is waived.
 * All source code is completely free to use by anyone for any purpose
 * with no restrictions whatsoever.
 *
 * For more information about the project, see: www.jonh.net/rulos
 *
 ************************************************************************/

#include "audio_server.h"

extern void syncdebug(uint8_t spaces, char f, uint16_t line);
//#define SYNCDEBUG()	syncdebug(0, 'U', __LINE__)
#define SYNCDEBUG()	{}

void ac_send_complete(SendSlot *sendSlot);


void aserv_recv(RecvSlot *recvSlot, uint8_t payload_len);
void _aserv_fetch_start(AudioServerAct *asa);
void _aserv_fetch_complete(AudioServerAct *asa);
void _aserv_start_play(AudioServerAct *asa);
void _aserv_advance(AudioServerAct *asa);

typedef void (AservAct_f)(AudioServerAct *asa);
static inline void _aserv_setup_act(AudioServer *aserv, AudioServerAct *act, AservAct_f *func)
{
	act->act.func = (ActivationFunc) func;
	act->aserv = aserv;
}

void init_audio_server(AudioServer *aserv, Network *network, uint8_t timer_id)
{
	_aserv_setup_act(aserv, &aserv->fetch_start, _aserv_fetch_start);
	_aserv_setup_act(aserv, &aserv->fetch_complete, _aserv_fetch_complete);
	_aserv_setup_act(aserv, &aserv->start_play, _aserv_start_play);
	_aserv_setup_act(aserv, &aserv->advance, _aserv_advance);

	aserv->recvSlot.func = aserv_recv;
	aserv->recvSlot.port = AUDIO_PORT;
	aserv->recvSlot.payload_capacity = sizeof(AudioRequestMessage);
	aserv->recvSlot.msg_occupied = FALSE;
	aserv->recvSlot.msg = (Message*) aserv->recv_msg_alloc;
	aserv->recvSlot.user_data = aserv;

	aserv->skip_cmd.token = sound_silence;
	aserv->loop_cmd.token = sound_silence;

	init_audio_streamer(&aserv->audio_streamer, timer_id);
	aserv->index_ready = FALSE;
	_aserv_fetch_start(&aserv->fetch_start);

	net_bind_receiver(network, &aserv->recvSlot);
}

void _aserv_fetch_start(AudioServerAct *asa)
{
	AudioServer *aserv = asa->aserv;

	SYNCDEBUG();

	// try to borrow sdc object from audio_streamer
	aserv->borrowed_sdc = as_borrow_sdc(&aserv->audio_streamer);
	if (aserv->borrowed_sdc==NULL)
	{
		// sdc not initialized yet
		SYNCDEBUG();
		schedule_us(10000, &aserv->fetch_start.act);
		return;
	}

	uint8_t *start_addr = (uint8_t*) &aserv->magic;
	uint16_t count = sizeof(aserv->magic) + sizeof(aserv->index);
	r_bool rc = sdc_start_transaction(
		aserv->borrowed_sdc,
		0,
		start_addr,
		count,
		&aserv->fetch_complete.act);
#if !SIM
		syncdebug(4, 'a', (uint16_t) (aserv));
		syncdebug(4, 'b', (uint16_t) (aserv->borrowed_sdc));
		syncdebug(4, 's', (uint16_t) (start_addr));
		syncdebug(4, 'c', (uint16_t) (count));
#endif
	if (!rc)
	{
		SYNCDEBUG();
		schedule_us(10000, &aserv->fetch_start.act);
		return;
	}
}

void _aserv_fetch_complete(AudioServerAct *asa)
{
	AudioServer *aserv = asa->aserv;
	SYNCDEBUG();
#if !SIM
	syncdebug(4, 'a', (uint16_t) (aserv));
	syncdebug(4, 'b', (uint16_t) (&aserv->borrowed_sdc));
	syncdebug(4, 'b', (uint16_t) (aserv->borrowed_sdc));
#endif
	aserv->index_ready = TRUE;
	sdc_end_transaction(aserv->borrowed_sdc, NULL);
}

void aserv_recv(RecvSlot *recvSlot, uint8_t payload_len)
{
	SYNCDEBUG();
	AudioServer *aserv = (AudioServer *) recvSlot->user_data;
	assert(payload_len == sizeof(AudioRequestMessage));
	AudioRequestMessage *arm = (AudioRequestMessage *) &recvSlot->msg->data;

	if (!aserv->index_ready)
	{
		SYNCDEBUG();
	}
	else
	{
		SYNCDEBUG();
		if (arm->skip)
		{
			_aserv_skip_to_clip(aserv, arm->skip_cmd, arm->loop_cmd);
		}
		else
		{
			aserv->loop_cmd = arm->loop_cmd;
		}
	}
	aserv->recvSlot.msg_occupied = FALSE;
}

// factored into a function as an entry point for debugging.
void _aserv_skip_to_clip(AudioServer *aserv, SoundCmd skip_cmd, SoundCmd loop_cmd)
{
	SYNCDEBUG();
	aserv->skip_cmd = skip_cmd;
	aserv->loop_cmd = loop_cmd;
	_aserv_start_play(&aserv->start_play);
}

void _aserv_start_play(AudioServerAct *asa)
{
	AudioServer *aserv = asa->aserv;
	SYNCDEBUG();
	if (aserv->skip_cmd.token==sound_silence)
	{
		SYNCDEBUG();
		as_stop_streaming(&aserv->audio_streamer);
	}
	else if (aserv->skip_cmd.token<0 || aserv->skip_cmd.token>=sound_num_tokens)
	{
		// error: invalid token.
		aserv->skip_cmd.token = sound_silence;
		as_stop_streaming(&aserv->audio_streamer);
		syncdebug(0, 'a', 0xfedc);
	}
	else
	{
		SYNCDEBUG();
#if !SIM
		syncdebug(4, 'd', (uint16_t) (&aserv->audio_streamer.sdc));
#endif
		syncdebug(4, 'o', aserv->audio_streamer.sdc.transaction_open);
		AuIndexRec *airec = &aserv->index[aserv->skip_cmd.token];
		r_bool rc = as_play(
			&aserv->audio_streamer,
			airec->start_offset,
			airec->block_offset,
			airec->end_offset,
			aserv->skip_cmd.volume,
			&aserv->advance.act);
		if (!rc)
		{
			SYNCDEBUG();
			// Retry rapidly, so we can get ahold of sdc as soon as it's
			// idle. (Yeah, I could have a callback from SD to alert the
			// next waiter, but what a big project. This'll do.)
			schedule_us(1, &aserv->start_play.act);
		}
	}
}

void _aserv_advance(AudioServerAct *asa)
{
	AudioServer *aserv = asa->aserv;
	aserv->skip_cmd = aserv->loop_cmd;
	_aserv_start_play(&aserv->start_play);
}
