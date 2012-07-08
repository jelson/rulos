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

/*
 * These are the base simulator modules, for just the clock and TWI.
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <time.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/timeb.h>
#include <signal.h>
#include <sched.h>
#include <ctype.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <errno.h>

#include "rocket.h"
#include "util.h"
#include "sim.h"

uint32_t f_cpu = 4000000;
uint8_t hal_initted = 0;

/**************** clock ****************/

sigset_t mask_set;

#define MAX_HANDLERS 20

typedef struct {
	Handler func;
	void *data;
} SimActivation_t;

static SimActivation_t simClockHandlers[MAX_HANDLERS];
static int numSimClockHandlers = 0;

static SimActivation_t simSIGIOHandlers[MAX_HANDLERS];
static int numSimSIGIOHandlers = 0;

static void sim_register_generic_handler(SimActivation_t *handlerList, int *numHandlers,
					 Handler func, void *data)
{
	uint8_t oldInt = hal_start_atomic();
	handlerList[*numHandlers].func = func;
	handlerList[*numHandlers].data = data;
	(*numHandlers)++;
	hal_end_atomic(oldInt);
}

void sim_register_clock_handler(Handler func, void *data)
{
	sim_register_generic_handler(simClockHandlers, &numSimClockHandlers,
				     func, data);
}


void sim_register_sigio_handler(Handler func, void *data)
{
	sim_register_generic_handler(simSIGIOHandlers, &numSimSIGIOHandlers,
				     func, data);
}

static void sim_generic_fire_handlers(SimActivation_t *handlerList, int numHandlers)
{
	uint8_t oldInt = hal_start_atomic();
	int i;

	for (i = 0; i < numHandlers; i++)
		handlerList[i].func(handlerList[i].data);
	hal_end_atomic(oldInt);
}

void sim_clock_handler(int signo)
{
	sim_generic_fire_handlers(simClockHandlers, numSimClockHandlers);
}

void sim_sigio_handler(int signo)
{
	sim_generic_fire_handlers(simSIGIOHandlers, numSimSIGIOHandlers);
}


uint32_t hal_start_clock_us(uint32_t us, Handler handler, void *data, uint8_t timer_id)
{
	assert(hal_initted == HAL_MAGIC); // did you forget to call hal_init()?
	
	/* init clock stuff */
	sigemptyset(&mask_set);
	sigaddset(&mask_set, SIGALRM);
	sigaddset(&mask_set, SIGIO);

	struct itimerval ivalue, ovalue;
	ivalue.it_interval.tv_sec = us/1000000;
	ivalue.it_interval.tv_usec = (us%1000000);
	ivalue.it_value = ivalue.it_interval;
	setitimer(ITIMER_REAL, &ivalue, &ovalue);

	sim_register_clock_handler(handler, data);
	signal(SIGALRM, sim_clock_handler);
	
	return us;
}

// this COULD be implemented with gettimeofday(), but I'm too lazy,
// since the only reason this function exists is for the wall clock
// app, so it only matters in hardware.
uint16_t hal_elapsed_milliintervals()
{
	return 0;
}

void hal_speedup_clock_ppm(int32_t ratio)
{
	// do nothing for now
}

uint8_t is_blocked = 0;

uint8_t hal_start_atomic()
{
	sigprocmask(SIG_BLOCK, &mask_set, NULL);
	uint8_t retval = is_blocked;
	is_blocked = 1;
	return retval;
}

void hal_end_atomic(uint8_t blocked)
{
	if (!blocked)
	{
		is_blocked = 0;
		sigprocmask(SIG_UNBLOCK, &mask_set, NULL);
	}
}

void hal_idle()
{
	// turns out 'man sleep' says sleep & sigalrm don't mix. yield is what we want.
	// No, sched_yield doesn't wait ANY time. libc suggests select()
	static struct timeval tv;
	tv.tv_sec = 0;
	tv.tv_usec = 100;	// .1 ms
	select(0, NULL, NULL, NULL, &tv);
}

void hal_delay_ms(uint16_t ms)
{
	static struct timeval tv;
	tv.tv_sec = ms/1000;
	tv.tv_usec = 1000*(ms%1000);
	select(0, NULL, NULL, NULL, &tv);
}

/********************** sensors *************************/

Handler _sensor_interrupt_handler = NULL;
void *_sensor_interrupt_data = NULL;
const uint32_t sensor_interrupt_simulator_counter_period = 507;
uint32_t sensor_interrupt_simulator_counter;

void sim_sensor_poll(void *data)
{
	sensor_interrupt_simulator_counter += 1;
	if (sensor_interrupt_simulator_counter == sensor_interrupt_simulator_counter_period)
	{
		if (_sensor_interrupt_handler != NULL)
		{
			_sensor_interrupt_handler(_sensor_interrupt_data);
		}
		sensor_interrupt_simulator_counter = 0;
	}

}


void sensor_interrupt_register_handler(Handler handler, void *data)
{
	_sensor_interrupt_handler = handler;
	_sensor_interrupt_data = data;
	sim_register_clock_handler(sim_sensor_poll, NULL);
}


/*************** twi ************************/

typedef struct {
	MediaStateIfc media;
	r_bool initted;
	MediaRecvSlot *mrs;
	int udp_socket;
} SimTwiState;
SimTwiState _g_sim_twi_state = { {NULL}, FALSE };



static void _sim_twi_send(MediaStateIfc *media,
	Addr dest_addr, char *data, uint8_t len,
	MediaSendDoneFunc sendDoneCB, void *sendDoneCBData);
static void sim_twi_poll(void *data);

MediaStateIfc *hal_twi_init(uint32_t speed_khz, Addr local_addr, MediaRecvSlot *mrs)
{
	SimTwiState *twi_state = &_g_sim_twi_state;
	twi_state->media.send = _sim_twi_send;
	twi_state->mrs = mrs;
	twi_state->udp_socket = socket(PF_INET, SOCK_DGRAM, 0);

	int rc;
	int on = 1;
	int flags = fcntl(twi_state->udp_socket, F_GETFL);
	rc = fcntl(twi_state->udp_socket, F_SETFL, flags | O_ASYNC);
	assert(rc==0);
	rc = ioctl(twi_state->udp_socket, FIOASYNC, &on );
	assert(rc==0);
	rc = ioctl(twi_state->udp_socket, FIONBIO, &on);
	assert(rc==0);
	int flag = -getpid();
	rc = ioctl(twi_state->udp_socket, SIOCSPGRP, &flag);
	assert(rc==0);

	struct sockaddr_in sai;
	sai.sin_family = AF_INET;
	sai.sin_addr.s_addr = htonl(INADDR_ANY);
	sai.sin_port = htons(SIM_TWI_PORT_BASE + local_addr);
	bind(twi_state->udp_socket, (struct sockaddr*)&sai, sizeof(sai));
	twi_state->initted = TRUE;
	sim_register_clock_handler(sim_twi_poll, NULL);
	sim_register_sigio_handler(sim_twi_poll, NULL);
	return &twi_state->media;
}


static void doRecvCallback(MediaRecvSlot *mrs)
{
	mrs->func(mrs, mrs->occupied_len);
}

static void sim_twi_poll(void *data)
{
	SimTwiState *twi_state = &_g_sim_twi_state;
	if (!twi_state->initted)
	{
		return;
	}

	char buf[4096];

	int rc = recv(
		twi_state->udp_socket,
		buf,
		sizeof(buf),
		MSG_DONTWAIT);

	if (rc<0)
	{
		assert(errno == EAGAIN);
		return;
	}

	assert(rc != 0);

	if (twi_state->mrs->occupied_len > 0)
	{
		LOGF((logfp, "TWI SIM: Packet arrived but network stack buffer busy; dropping\n"));
		return;
	}

	if (rc > twi_state->mrs->capacity)
	{
		LOGF((logfp, "TWI SIM: Discarding %d-byte packet; too long for net stack's buffer\n", rc));
		return;
	}

	twi_state->mrs->occupied_len = rc;
	memcpy(twi_state->mrs->data, buf, rc);
	schedule_now((ActivationFuncPtr) doRecvCallback, twi_state->mrs);
}


typedef struct {
	MediaSendDoneFunc sendDoneCB;
	void *sendDoneCBData;
} sendCallbackAct_t;

sendCallbackAct_t sendCallbackAct_g;

static void doSendCallback(sendCallbackAct_t *sca)
{
	sca->sendDoneCB(sca->sendDoneCBData);
}

static void _sim_twi_send(MediaStateIfc *media,
			  Addr dest_addr, char *data, uint8_t len,
			  MediaSendDoneFunc sendDoneCB, void *sendDoneCBData)
{
	SimTwiState *twi_state = (SimTwiState *) media;
/*
	LOGF((logfp, "hal_twi_send_byte(%02x [%c])\n",
		byte,
		(byte>=' ' && byte<127) ? byte : '_'));
*/
	
	struct sockaddr_in sai;

	sai.sin_family = AF_INET;
	sai.sin_addr.s_addr = htonl(0x7f000001);
	sai.sin_port = htons(SIM_TWI_PORT_BASE + dest_addr);
	sendto(twi_state->udp_socket, data, len,
		   0, (struct sockaddr*)&sai, sizeof(sai));

	if (sendDoneCB)
	{
		sendCallbackAct_g.sendDoneCB = sendDoneCB;
		sendCallbackAct_g.sendDoneCBData = sendDoneCBData;
		schedule_now((ActivationFuncPtr) doSendCallback, &sendCallbackAct_g);
	}
}

/* recv-only uart simulator */

int sim_uart_fd[2];

void _sim_uart_recv(void *data)
{
	UartHandler* uart_handler = (UartHandler*) data;
	char c;
	int rc = read(sim_uart_fd[uart_handler->uart_id], &c, 1);
	if (rc==1)
	{
		//fprintf(stderr, "_sim_uart_recv(%c)\n", c);
		(uart_handler->recv)(uart_handler, c);
		schedule_us(1000, _sim_uart_recv, uart_handler);
	}
	else
	{
		fprintf(stderr, "EOF on sim_uart_fd\n");
	}
}

void hal_uart_init(UartHandler* handler, uint32_t baud, r_bool stop2, uint8_t uart_id)
{
	handler->uart_id = uart_id;
	assert(uart_id<sizeof(sim_uart_fd)/sizeof(sim_uart_fd[0]));
	sim_uart_fd[uart_id] = open("sim_uart", O_RDONLY);
	fprintf(stderr, "open returns %d\n", sim_uart_fd[uart_id]);
	schedule_us(1000, _sim_uart_recv, handler);
}

void hal_uart_start_send(UartHandler* handler)
{
	// TODO neutered; won't call your more-chars upcall ever
}

/************ init ***********************/


FILE *logfp = NULL;

void hal_uart_sync_send(char *s, uint8_t len)
{
	char buf[1000];
	memcpy(buf, s, len);
	buf[len] = '\0';
	LOGF((logfp, "uart send: %s\n", buf));
}

void hal_init()
{
	logfp = fopen("log", "w");
	signal(SIGIO, sim_sigio_handler);
	hal_initted = HAL_MAGIC;
}
