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

#include <stdbool.h>

#include "periph/audio/audio_server.h"

#define FETCH_RETRY_TIME	1000000

extern void syncdebug(uint8_t spaces, char f, uint16_t line);
#define SYNCPRINT(x,y,z)	syncdebug(x,y,z)
//#define SYNCPRINT(x,y,z)	{}
#define SYNCDEBUG()	SYNCPRINT(0, 'U', __LINE__)

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

	uint8_t stream_id;
	for (stream_id=0; stream_id<AUDIO_NUM_STREAMS; stream_id++)
	{
		aserv->audio_stream[stream_id].skip_cmd.token = sound_silence;
		aserv->audio_stream[stream_id].loop_cmd.token = sound_silence;
		aserv->audio_stream[stream_id].mlvolume = 0;
	}
	aserv->audio_stream[AUDIO_STREAM_BACKGROUND].mlvolume = 8;

	aserv->music_random_seeded = FALSE;
	// none of these should be accessed until index ready,
	// but might as well load sane-ish values
	aserv->num_music_tokens = 1;
	aserv->music_first_token = sound_silence;

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
		schedule_us(FETCH_RETRY_TIME, (ActivationFuncPtr) _aserv_fetch_start, aserv);
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
		SYNCPRINT(4, 'a', (uint16_t) (aserv));
		SYNCPRINT(4, 'b', (uint16_t) (aserv->borrowed_sdc));
		SYNCPRINT(4, 's', (uint16_t) (start_addr));
		SYNCPRINT(4, 'c', (uint16_t) (count));
#endif
	if (!rc)
	{
		SYNCDEBUG();
		schedule_us(FETCH_RETRY_TIME, (ActivationFuncPtr) _aserv_fetch_start, aserv);
		return;
	}
}

void _aserv_fetch_complete(AudioServer *aserv)
{
	SYNCDEBUG();
#if !SIM
	SYNCPRINT(4, 'a', (uint16_t) (aserv));
	SYNCPRINT(4, 'b', (uint16_t) (&aserv->borrowed_sdc));
	SYNCPRINT(4, 'b', (uint16_t) (aserv->borrowed_sdc));
#endif

	bool success = !aserv->borrowed_sdc->error;
	if (success)
	{
		int i;
		for (i=0; i<sound_num_tokens; i++)
		{
			SYNCPRINT(0, 'i', i);
			SYNCPRINT(2, 'o', aserv->index[i].start_offset);
			if (aserv->index[i].is_disco)
			{
				aserv->music_first_token = i;
				break;
			}
		}
		for (i=aserv->music_first_token; i<sound_num_tokens; i++)
		{
			SYNCPRINT(0, 'j', i);
			if (!aserv->index[i].is_disco)
			{
				aserv->num_music_tokens = i-aserv->music_first_token;
				break;
			}
		}
		if (i==sound_num_tokens)
		{
			aserv->num_music_tokens = i-aserv->music_first_token;
		}

		aserv->index_ready = TRUE;
	}

	sdc_end_transaction(aserv->borrowed_sdc, NULL, NULL);

	if (!success)
	{
		SYNCDEBUG();
		schedule_us(FETCH_RETRY_TIME, (ActivationFuncPtr) _aserv_fetch_start, aserv);
	}
	SYNCDEBUG();
}

void _aserv_skip_stream(AudioServer *aserv, uint8_t stream_id)
{
	SYNCDEBUG();
	if (
		aserv->audio_stream[stream_id].skip_cmd.token != sound_silence
		&& stream_id >= aserv->active_stream)
	{
		SYNCDEBUG();
		// preemption
		aserv->active_stream = stream_id;
		_aserv_start_play(aserv);
	}
	else if (
		aserv->audio_stream[stream_id].skip_cmd.token == sound_silence
		&& stream_id == aserv->active_stream)
	{
		SYNCDEBUG();
		// drop back down the stack and find something else to pick up
		_aserv_advance(aserv);
	}
}

void aserv_recv_arm(RecvSlot *recvSlot, uint8_t payload_len)
{
	SYNCDEBUG();
	AudioServer *aserv = (AudioServer *) recvSlot->user_data;
	assert(payload_len == sizeof(AudioRequestMessage));
	AudioRequestMessage *arm = (AudioRequestMessage *) &recvSlot->msg->data;
	assert(arm->stream_id < AUDIO_NUM_STREAMS);

	aserv->audio_stream[arm->stream_id].loop_cmd = arm->loop_cmd;
	if (arm->skip)
	{
		SYNCDEBUG();
		aserv->audio_stream[arm->stream_id].skip_cmd = arm->skip_cmd;
		SYNCPRINT(2, 'i', arm->stream_id);
		SYNCPRINT(2, 'k', aserv->audio_stream[arm->stream_id].skip_cmd.token);
		SYNCPRINT(2, 'l', aserv->audio_stream[arm->stream_id].loop_cmd.token);
		_aserv_skip_stream(aserv, arm->stream_id);
	}

	recvSlot->msg_occupied = FALSE;
}

void aserv_recv_avm(RecvSlot *recvSlot, uint8_t payload_len)
{
	SYNCDEBUG();
	AudioServer *aserv = (AudioServer *) recvSlot->user_data;
	assert(payload_len == sizeof(AudioVolumeMessage));
	AudioVolumeMessage *avm = (AudioVolumeMessage *) &recvSlot->msg->data;

	assert(avm->stream_id < AUDIO_NUM_STREAMS);
	aserv->audio_stream[avm->stream_id].mlvolume = avm->mlvolume;
	if (aserv->active_stream == avm->stream_id)
	{
		as_set_volume(&aserv->audio_streamer, avm->mlvolume);
	}

	recvSlot->msg_occupied = FALSE;
}

void aserv_recv_mcm(RecvSlot *recvSlot, uint8_t payload_len)
{
	SYNCDEBUG();
	AudioServer *aserv = (AudioServer *) recvSlot->user_data;
	assert(payload_len == sizeof(MusicControlMessage));
	MusicControlMessage *mcm = (MusicControlMessage *) &recvSlot->msg->data;

	if (aserv->index_ready)
	{
		if (!aserv->music_random_seeded)
		{
			SYNCPRINT(0, 's', 0x5eed);
			// first use? jump to a random song. random seed is 1/10ths of sec since boot.
			aserv->music_offset = (SoundToken) (
					((clock_time_us()/100000) % (aserv->num_music_tokens)));
			aserv->music_random_seeded = TRUE;
		}

		// now advance one or retreat one, based on request
		SYNCPRINT(1, 'o', aserv->music_offset);
		aserv->music_offset =
			(aserv->music_offset + mcm->advance + aserv->num_music_tokens)
			% (aserv->num_music_tokens);
		SYNCPRINT(1, 'o', aserv->music_offset);
		aserv->audio_stream[AUDIO_STREAM_MUSIC].skip_cmd.token =
			(SoundToken) (aserv->music_offset + aserv->music_first_token);

		SYNCPRINT(1, 'd', mcm->advance);
		SYNCPRINT(1, 't', aserv->music_offset);
		SYNCPRINT(1, 'n', aserv->num_music_tokens);
		// and start it playin'
		_aserv_skip_stream(aserv, AUDIO_STREAM_MUSIC);
	}

	recvSlot->msg_occupied = FALSE;
}

void _aserv_dbg_play(AudioServer *aserv, SoundToken skip, SoundToken loop)
{
	aserv->audio_stream[AUDIO_STREAM_BURST_EFFECTS].skip_cmd.token = skip;
	aserv->audio_stream[AUDIO_STREAM_BURST_EFFECTS].loop_cmd.token = loop;
	aserv->audio_stream[AUDIO_STREAM_BURST_EFFECTS].mlvolume = 0;
	_aserv_start_play(aserv);
}

void _aserv_start_play(AudioServer *aserv)
{
	SYNCDEBUG();
	AudioEffectsStream *stream = &aserv->audio_stream[aserv->active_stream];
	if (stream->skip_cmd.token==sound_silence)
	{
		SYNCDEBUG();
		as_stop_streaming(&aserv->audio_streamer);
	}
	else if (stream->skip_cmd.token<0 || stream->skip_cmd.token>=sound_num_tokens)
	{
		// error: invalid token.
		stream->skip_cmd.token = sound_silence;
		as_stop_streaming(&aserv->audio_streamer);
		SYNCPRINT(0, 'a', 0xfedc);
	}
	else if (!aserv->index_ready)
	{
		SYNCDEBUG();
	}
	else
	{
		SYNCDEBUG();
#if !SIM
//		SYNCPRINT(4, 'd', (uint16_t) (&aserv->audio_streamer.sdc));
#endif
//		SYNCPRINT(4, 'o', aserv->audio_streamer.sdc.transaction_open);

		AuIndexRec *airec = &aserv->index[stream->skip_cmd.token];
//		SYNCPRINT(4, 's', airec->start_offset>>16);
//		SYNCPRINT(4, 's', airec->start_offset&0xffff);
//		SYNCPRINT(4, 'e', airec->end_offset>>16);
//		SYNCPRINT(4, 'e', airec->end_offset&0xffff);
		SYNCPRINT(5, 'i', aserv->active_stream);
		SYNCPRINT(5, 't', stream->skip_cmd.token);
		SYNCPRINT(5, 'v', stream->mlvolume);
		as_set_volume(&aserv->audio_streamer, stream->mlvolume);
		r_bool rc = as_play(
			&aserv->audio_streamer,
			airec->start_offset,
			airec->block_offset,
			airec->end_offset,
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
	aserv->audio_stream[aserv->active_stream].skip_cmd =
		aserv->audio_stream[aserv->active_stream].loop_cmd;
	// drop down the stream stack until we find some non-silence.
	for ( ; 
		aserv->active_stream >= 0 &&
		aserv->audio_stream[aserv->active_stream].skip_cmd.token == sound_silence;
		aserv->active_stream--)
	{ }

	if (aserv->active_stream < 0)
	{
		aserv->active_stream = 0;
	}

	_aserv_start_play(aserv);
}
