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


void aserv_recv_arm(RecvSlot *recvSlot, uint8_t payload_len);
void aserv_recv_avm(RecvSlot *recvSlot, uint8_t payload_len);
void aserv_recv_mcm(RecvSlot *recvSlot, uint8_t payload_len);
void _aserv_fetch_start(AudioServer *as);
void _aserv_fetch_complete(AudioServer *as);
void _aserv_start_play(AudioServer *as);
void _aserv_advance(AudioServer *as);

void init_audio_server(AudioServer *aserv, Network *network, uint8_t timer_id)
{
	aserv->arm_recvSlot.func = aserv_recv_arm;
	aserv->arm_recvSlot.port = AUDIO_PORT;
	aserv->arm_recvSlot.payload_capacity = sizeof(AudioRequestMessage);
	aserv->arm_recvSlot.msg_occupied = FALSE;
	aserv->arm_recvSlot.msg = (Message*) aserv->arm_recv_msg_alloc;
	aserv->arm_recvSlot.user_data = aserv;

	aserv->avm_recvSlot.func = aserv_recv_avm;
	aserv->avm_recvSlot.port = SET_VOLUME_PORT;
	aserv->avm_recvSlot.payload_capacity = sizeof(AudioVolumeMessage);
	aserv->avm_recvSlot.msg_occupied = FALSE;
	aserv->avm_recvSlot.msg = (Message*) aserv->avm_recv_msg_alloc;
	aserv->avm_recvSlot.user_data = aserv;

	aserv->mcm_recvSlot.func = aserv_recv_mcm;
	aserv->mcm_recvSlot.port = MUSIC_CONTROL_PORT;
	aserv->mcm_recvSlot.payload_capacity = sizeof(AudioVolumeMessage);
	aserv->mcm_recvSlot.msg_occupied = FALSE;
	aserv->mcm_recvSlot.msg = (Message*) aserv->mcm_recv_msg_alloc;
	aserv->mcm_recvSlot.user_data = aserv;

	aserv->skip_cmd.token = sound_silence;
	aserv->loop_cmd.token = sound_silence;

	aserv->music_random_seeded = FALSE;
	// none of these should be accessed until index ready,
	// but might as well load sane-ish values
	aserv->cur_music_token = 0;
	aserv->num_music_tokens = 1;
	aserv->music_token_offset = sound_silence;

	init_audio_streamer(&aserv->audio_streamer, timer_id);
	aserv->index_ready = FALSE;
	_aserv_fetch_start(aserv);

	net_bind_receiver(network, &aserv->arm_recvSlot);
	net_bind_receiver(network, &aserv->avm_recvSlot);
	net_bind_receiver(network, &aserv->mcm_recvSlot);
}

void _aserv_fetch_start(AudioServer *aserv)
{
	SYNCDEBUG();

	// try to borrow sdc object from audio_streamer
	aserv->borrowed_sdc = as_borrow_sdc(&aserv->audio_streamer);
	if (aserv->borrowed_sdc==NULL)
	{
		// sdc not initialized yet
		SYNCDEBUG();
		schedule_us(10000, (ActivationFuncPtr) _aserv_fetch_start, aserv);
		return;
	}

	uint8_t *start_addr = (uint8_t*) &aserv->magic;
	uint16_t count = sizeof(aserv->magic) + sizeof(aserv->index);
	r_bool rc = sdc_start_transaction(
		aserv->borrowed_sdc,
		0,
		start_addr,
		count,
		(ActivationFuncPtr) _aserv_fetch_complete,
		aserv);
#if !SIM
		syncdebug(4, 'a', (uint16_t) (aserv));
		syncdebug(4, 'b', (uint16_t) (aserv->borrowed_sdc));
		syncdebug(4, 's', (uint16_t) (start_addr));
		syncdebug(4, 'c', (uint16_t) (count));
#endif
	if (!rc)
	{
		SYNCDEBUG();
		schedule_us(10000, (ActivationFuncPtr) _aserv_fetch_start, aserv);
		return;
	}
}

void _aserv_fetch_complete(AudioServer *aserv)
{
	SYNCDEBUG();
#if !SIM
	syncdebug(4, 'a', (uint16_t) (aserv));
	syncdebug(4, 'b', (uint16_t) (&aserv->borrowed_sdc));
	syncdebug(4, 'b', (uint16_t) (aserv->borrowed_sdc));
#endif

	int i;
	for (i=0; i<sound_num_tokens; i++)
	{
		syncdebug(0, 'i', i);
		if (aserv->index[i].is_disco)
		{
			aserv->music_token_offset = i;
			break;
		}
	}
	for (i=aserv->music_token_offset; i<sound_num_tokens; i++)
	{
		syncdebug(0, 'j', i);
		if (!aserv->index[i].is_disco)
		{
			aserv->num_music_tokens = i-aserv->music_token_offset;
			break;
		}
	}
	if (i==sound_num_tokens)
	{
		aserv->num_music_tokens = i-aserv->music_token_offset;
	}

	aserv->index_ready = TRUE;
	sdc_end_transaction(aserv->borrowed_sdc, NULL, NULL);
}

void aserv_recv_arm(RecvSlot *recvSlot, uint8_t payload_len)
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
	recvSlot->msg_occupied = FALSE;
}

void aserv_recv_avm(RecvSlot *recvSlot, uint8_t payload_len)
{
	SYNCDEBUG();
	AudioServer *aserv = (AudioServer *) recvSlot->user_data;
	assert(payload_len == sizeof(AudioVolumeMessage));
	AudioVolumeMessage *avm = (AudioVolumeMessage *) &recvSlot->msg->data;

	as_set_music_volume(&aserv->audio_streamer, avm->music_mlvolume);
	recvSlot->msg_occupied = FALSE;
}

void aserv_recv_mcm(RecvSlot *recvSlot, uint8_t payload_len)
{
	SYNCDEBUG();
	AudioServer *aserv = (AudioServer *) recvSlot->user_data;
	assert(payload_len == sizeof(MusicControlMessage));
	MusicControlMessage *mcm = (MusicControlMessage *) &recvSlot->msg->data;

	if (!aserv->index_ready)
	{
		SYNCDEBUG();
	}
	else
	{
		if (!aserv->music_random_seeded)
		{
			syncdebug(0, 's', 0x5eed);
			// first use? jump to a random song. random seed is 1/10ths of sec since boot.
			aserv->cur_music_token =
				(clock_time_us()/100000)
				% (aserv->num_music_tokens);
			aserv->music_random_seeded = TRUE;
		}
		// now advance one or retreat one, based on request
		aserv->cur_music_token =
			(aserv->cur_music_token + mcm->advance + aserv->num_music_tokens)
			% (aserv->num_music_tokens);
		syncdebug(0, 'd', mcm->advance);
		syncdebug(0, 't', aserv->cur_music_token);
		syncdebug(0, 'n', aserv->num_music_tokens);
		// and start it playin'
		SoundCmd music_cmd;
		music_cmd.token = aserv->cur_music_token + aserv->music_token_offset;
		SoundCmd silence_cmd;
		silence_cmd.token = sound_silence;
		_aserv_skip_to_clip(aserv, music_cmd, silence_cmd);
	}
	recvSlot->msg_occupied = FALSE;
}

// factored into a function as an entry point for debugging.
void _aserv_skip_to_clip(AudioServer *aserv, SoundCmd skip_cmd, SoundCmd loop_cmd)
{
	SYNCDEBUG();
	aserv->skip_cmd = skip_cmd;
	aserv->loop_cmd = loop_cmd;
	_aserv_start_play(aserv);
}

void _aserv_start_play(AudioServer *aserv)
{
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
		syncdebug(4, 's', airec->start_offset>>16);
		syncdebug(4, 's', airec->start_offset&0xffff);
		syncdebug(4, 'e', airec->end_offset>>16);
		syncdebug(4, 'e', airec->end_offset&0xffff);
		r_bool rc = as_play(
			&aserv->audio_streamer,
			airec->start_offset,
			airec->block_offset,
			airec->end_offset,
			airec->is_disco,	/* is_music -> tells whether to apply music volume attenuation */
			(ActivationFuncPtr) _aserv_advance,
			aserv);
		if (!rc)
		{
			SYNCDEBUG();
			// Retry rapidly, so we can get ahold of sdc as soon as it's
			// idle. (Yeah, I could have a callback from SD to alert the
			// next waiter, but what a big project. This'll do.)
			schedule_us(1, (ActivationFuncPtr) _aserv_start_play, aserv);
		}
	}
}

void _aserv_advance(AudioServer *aserv)
{
	aserv->skip_cmd = aserv->loop_cmd;
	_aserv_start_play(aserv);
}
