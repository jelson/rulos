//#define TIME_DEBUG
//#define MANUAL_US_TEST
//#define NO_GYRO
//#define NO_ANALOG

#include <string.h>
#include <avr/interrupt.h>

#define F_CPU 8000000UL
#include <util/delay.h>

#include "rocket.h"
#include "hardware.h"
#include "funcseq.h"
#include "pov.h"
#include "vect3d.h"
#include "tilty_input.h"

struct locatorAct;
typedef struct locatorAct locatorAct_t;

typedef void (*PeripheralSendCompleteFunc)(locatorAct_t *aa);
typedef void (*PeripheralRecvCompleteFunc)(locatorAct_t *aa, char *data, int len);

typedef struct {
	ActivationFunc f;
	char *messages[5];
	int cur_index;
	PovAct *povAct;
} MessageChangeAct;

void message_change_func(MessageChangeAct *mca)
{
	mca->cur_index += 1;
	char *msg = mca->messages[mca->cur_index];
	if (msg==NULL)
	{
		mca->cur_index = 0;
		msg = mca->messages[mca->cur_index];
	}
	pov_write(mca->povAct, msg);

	schedule_us(1000000, (Activation*) mca);
}

void message_change_init(MessageChangeAct *mca, PovAct *povAct)
{
	mca->f = (ActivationFunc) message_change_func;
	mca->messages[0] = " ACE";
	mca->messages[1] = "  OF";
	mca->messages[2] = "CLUBS";
	mca->messages[3] = "";
	mca->messages[4] = NULL;
	mca->cur_index = 0;
	mca->povAct = povAct;
	schedule_us(1000000, (Activation*) mca);
}

struct locatorAct {
	ActivationFunc f;

	FuncSeq funcseq;

	// uart
	char UARTsendBuf[48];
	uint8_t uartSending;

	struct wait_uart_act_t {
		ActivationFunc f;
		locatorAct_t *aa;
	} wait_uart_act;

	char debug_state;

	// twi
	char TWIsendBuf[10];
	char TWIrecvBuf[10];
	TWIRecvSlot *trs;
	PeripheralSendCompleteFunc sendFunc;
	PeripheralRecvCompleteFunc recvFunc;
	uint8_t twiAddr;
	uint8_t readLen;

	Vect3D accel;
	Vect3D gyro;

	PovAct pov_act;
	MessageChangeAct message_change;
};

static locatorAct_t aa_g;

#define ACCEL_ADDR 0b0111000
#define ACCEL_SAMPLING_RATE_BITS 0b001 // 50 hz
#define ACCEL_RANGE_BITS 0b01 // +/- 4g

#define GYRO_ADDR  0b1101000
#define GYRO_FS_SEL 0x3 // 2000 deg/sec
#define GYRO_DLPF_CFG 0x3 // 42 hz

#define FIRMWARE_ID "$Rev$"
#define ID_OFFSET 6

#define GPIO_10V_ENABLE GPIO_B0
#define GPIO_US_XMIT    GPIO_D6
#define GPIO_US_RECV    GPIO_C6
#define US_RECV_CHANNEL 0
#define US_QUIET_TRAINING_LEN 2000
#define US_MIN_NOISE_RANGE    25
#define US_MIN_RUN_LENGTH     5
#define US_CHIRP_LEN_US       1000
#define US_CHIRP_RING_TIME_MS 40

/****************************************************/


/***** serial port helpers ******/

static inline void uartSendDone(void *cb_data)
{
	locatorAct_t *aa = (locatorAct_t *) cb_data;
	aa->uartSending = FALSE;
}

static inline void emit(locatorAct_t *aa, char *s)
{
	aa->uartSending = TRUE;
	uart_send(RULOS_UART0, s, strlen(s), uartSendDone, aa);
}



/*** sending data to a locator peripheral ***/

void _peripheralSendComplete(void *user_data)
{
	locatorAct_t *aa = (locatorAct_t *) user_data;
	aa->sendFunc(aa);
}

void sendToPeripheral(locatorAct_t *aa,
					  uint8_t twiAddr,
					  char *data,
					  uint8_t len,
					  PeripheralSendCompleteFunc f)
{
	aa->sendFunc = f;
	hal_twi_send(twiAddr, data, len, _peripheralSendComplete, aa);
}


/*** receiving data from a locator peripheral, using the
	 write-address-then-read protocol ***/

void _locatorReadComplete(TWIRecvSlot *trs, uint8_t len)
{
	locatorAct_t *aa = (locatorAct_t *) trs->user_data;
	aa->recvFunc(aa, trs->data, aa->readLen);
}

void _readFromPeripheral2(locatorAct_t *aa)
{
	aa->trs = (TWIRecvSlot *) aa->TWIrecvBuf;
	aa->trs->func = _locatorReadComplete;
	aa->trs->capacity = aa->readLen;
	aa->trs->user_data = aa;
	hal_twi_read(aa->twiAddr, aa->trs);
}


void readFromPeripheral(locatorAct_t *aa,
						uint8_t twiAddr,
						uint16_t baseAddr,
						int len,
						PeripheralRecvCompleteFunc f)
{
	aa->recvFunc = f;
	aa->readLen = len;

	aa->twiAddr = twiAddr;
	aa->TWIsendBuf[0] = baseAddr;
	sendToPeripheral(aa, twiAddr, aa->TWIsendBuf, 1, _readFromPeripheral2);
}


/*************************************/

void remote_debug(char *msg)
{
	if (!aa_g.uartSending) {
		emit(&aa_g, msg);
	}
}

typedef struct
{
	UIEventHandlerFunc func;
	struct locatorAct *la;
	PovAct *pov;
	MessageChangeAct *mca;
	uint8_t card_suit;
	uint8_t card_value;
	uint8_t *mode_ptr;
} TiltyInputHandler;

static char *card_value_names[] = {
	"ZERO",
	"ACE",
	"TWO",
	"THREE",
	"FOUR",
	"FIVE",
	"SIX",
	"SEVEN",
	"EIGHT",
	"NINE",
	"TEN",
	"JACK",
	"KING",
	"QUEEN",
	"FORTN",
	"FIFTN"
	};
static char *card_suit_names[] = {
	"HEARTS",
	"DIAMONDS",
	"CLUBS",
	"SPADES"
	};

void tih_handle(TiltyInputHandler *tih, UIEvent evt)
{
	switch (evt)
	{
		case ti_enter_pov:
			tih->mca->messages[0] = card_value_names[tih->card_value];
			tih->mca->messages[2] = card_suit_names[tih->card_suit];
			pov_set_visible(tih->pov, TRUE);
			break;
		case ti_exit_pov:
			pov_set_visible(tih->pov, FALSE);
			tih->card_suit = 0;
			tih->card_value = 0;
			tih->mode_ptr = &tih->card_suit;
			break;
		case ti_up:
			tih->mode_ptr = &tih->card_suit;
			break;
		case ti_down:
			tih->mode_ptr = &tih->card_value;
			break;
		case ti_left:
			(*(tih->mode_ptr)) = ((*(tih->mode_ptr))<<1) | 1;
			break;
		case ti_right:
			(*(tih->mode_ptr)) = ((*(tih->mode_ptr))<<1) | 0;
			break;
	}

	tih->card_value &= 0x0f;
	tih->card_suit &= 0x03;


	if (!tih->la->uartSending) {
		static char buf[80];
		snprintf(buf, sizeof(buf)-1, "Input event 0x%02x %d %s of %d %s mode %s\r\n",
			evt,
			tih->card_value, card_value_names[tih->card_value],
			tih->card_suit, card_suit_names[tih->card_suit],
			(tih->mode_ptr==&tih->card_suit) ? "suit" : "value"
			);
		emit(tih->la, buf);
	}
}

void tih_init(TiltyInputHandler *tih, struct locatorAct *la, PovAct *pov, MessageChangeAct *mca)
{
	tih->func = (UIEventHandlerFunc) tih_handle;
	tih->la = la;
	tih->pov = pov;
	tih->mca = mca;
	tih->card_suit = 0;
	tih->card_value = 0;
	tih->mode_ptr = &tih->card_suit;
}

/*************************************/

static inline void seq_emit_done(void *cb_data)
{
	locatorAct_t *aa = (locatorAct_t *) cb_data;
	aa->uartSending = FALSE;
	funcseq_next(&aa->funcseq);
}

static inline void seq_emit(locatorAct_t *aa, char *s)
{
	aa->uartSending = TRUE;
	uart_send(RULOS_UART0, s, strlen(s), seq_emit_done, aa);
}


static void config_next(locatorAct_t *aa)
{
	aa_g.debug_state = 'c';
	funcseq_next(&aa->funcseq);
}

static inline void config_recv_next(locatorAct_t *aa, char *data, int len)
{
	// Just here to have matching parameter types;
	// we let the receiver slurp the data directly out of the
	// values already stashed in aa.
	config_next(aa);
}

// This is a little clumsy, but keeps the sequencing code otherwise clean-ish.
static inline char *last_read_data(locatorAct_t *aa)
{
	return aa->trs->data;
}

// Configure the accelerometer by first reading config byte 0x14, then
// setting the low bits (leaving the 3 highest bits untouched), and
// writing it back.  This callback is called when the initial read
// completes.


void wait_uart_done(struct wait_uart_act_t *wua)
{
	if (wua->aa->uartSending)
	{
		aa_g.debug_state = 'a';
		schedule_us(1000, (Activation*) wua);
	}
	else
	{
		aa_g.debug_state = 'b';
		config_next(wua->aa);
	}
}

void wait_uart(locatorAct_t *aa)
{
	aa_g.debug_state = 'B';
	aa->wait_uart_act.f = (ActivationFunc) wait_uart_done;
	aa->wait_uart_act.aa = aa;
	schedule_us(1000, (Activation*) &aa->wait_uart_act);
}

void say1(locatorAct_t *aa)
{
	aa_g.debug_state = 'C';
	seq_emit(aa, "Line 1!\r\n");
}

void say2(locatorAct_t *aa)
{
	aa_g.debug_state = 'D';
	seq_emit(aa, "Another thing!\r\n");
}

void say3(locatorAct_t *aa)
{
	aa_g.debug_state = 'E';
	seq_emit(aa, "A totally unrelated thing!\r\n");
}

void configLocator_acc1(void *v_aa)
{
	aa_g.debug_state = 'F';
	locatorAct_t *aa = (locatorAct_t *) v_aa;
	readFromPeripheral(aa, ACCEL_ADDR, 0x14, 1, config_recv_next);
}

void configLocator_acc2(void *v_aa)
{
	locatorAct_t *aa = (locatorAct_t *) v_aa;
	aa->TWIsendBuf[0] = 0x14;
	aa->TWIsendBuf[1] =
		(last_read_data(aa)[0] & 0b11100000) |
		(ACCEL_RANGE_BITS << 3) |
		(ACCEL_SAMPLING_RATE_BITS);
	sendToPeripheral(aa, ACCEL_ADDR, aa->TWIsendBuf, 2, config_next);
}

// Next configure the gyro by writing the full-scale deflection and
// sample-rate bits to register 0x16.
void configLocator_gyro(void *v_aa)
{
	locatorAct_t *aa = (locatorAct_t *) v_aa;
	aa->TWIsendBuf[0] = 0x16;
	aa->TWIsendBuf[1] =
		(GYRO_FS_SEL << 3) |
		(GYRO_DLPF_CFG);
	sendToPeripheral(aa, GYRO_ADDR, aa->TWIsendBuf, 2, config_next);
}

// all done!  start sampling.
void configLocator_done(void *v_aa)
{
	locatorAct_t *aa = (locatorAct_t *) v_aa;
	schedule_now((Activation *) aa);
}


/*************************************/

static inline int16_t convert_accel(char lsb, char msb)
{
	int16_t mag = ((((int16_t) msb) & 0b01111111) << 2) | ((lsb & 0b11000000) >> 6);
	
	if (msb & 0b10000000)
		mag = mag-512;

	return mag;
}

void sampleLocator3(locatorAct_t *aa, char *data, int len)
{
	// convert and send the data out of serial
	aa->gyro.x = ((int16_t) data[0]) << 8 | data[1];
	aa->gyro.y = ((int16_t) data[2]) << 8 | data[3];
	aa->gyro.z = ((int16_t) data[4]) << 8 | data[5];

/*
	if (!aa->uartSending) {
		snprintf(aa->UARTsendBuf, sizeof(aa->UARTsendBuf)-1, "^g;x=%5d;y=%5d;z=%5d$\r\n", x, y, z);
		emit(aa, aa->UARTsendBuf);
	}
*/
}

void sampleLocator2(locatorAct_t *aa, char *data, int len)
{
	// read 6 bytes from gyro over TWI starting from address 0x2
	readFromPeripheral(aa, GYRO_ADDR, 0x1D, 6, sampleLocator3);

	// convert and send the accelerometer data we just received out to
	// the serial port
	Vect3D *accel = &aa->accel;
	accel->x = convert_accel(data[0], data[1]);
	accel->y = convert_accel(data[2], data[3]);
	accel->z = convert_accel(data[4], data[5]);

	r_bool wave_positive = accel->x > 0;
	char dbg_zcross = '_';
	if (!(aa->pov_act.last_wave_positive) && wave_positive)
	{
		// zero-crossing.
		pov_measure(&aa->pov_act);
		dbg_zcross = 'Z';
	}
	aa->pov_act.last_wave_positive = wave_positive;

	if (!aa->uartSending) {
/*
		char *msg = aa_g.message_change.messages[aa_g.message_change.cur_index];
		snprintf(aa->UARTsendBuf, sizeof(aa->UARTsendBuf)-1, "per %ld %c '%s'\r\n",
			aa->pov_act.lastPeriod,
			dbg_zcross,
			msg
			);
		emit(aa, aa->UARTsendBuf);
*/
	}

}
	
	
void sampleLocator(locatorAct_t *aa)
{
	// schedule reading of the next sample
	schedule_us(5000, (Activation *) aa);

	// see if we've received any serial commands
	char cmd;
	while (CharQueue_pop(uart_recvq(RULOS_UART0)->q, &cmd)) {
		switch (cmd) {
		}
	}

	// read 6 bytes from accelerometer starting from address 0x2
	readFromPeripheral(aa, ACCEL_ADDR, 0x2, 6, sampleLocator2);
}

SequencableFunc func_array[] = {
	(SequencableFunc) wait_uart,
	(SequencableFunc) say1,
	(SequencableFunc) wait_uart,
	(SequencableFunc) say2,
	(SequencableFunc) wait_uart,
	(SequencableFunc) say3,
	configLocator_acc1,
	configLocator_acc2,
	configLocator_gyro,
	configLocator_done,
	NULL
	};

int main()
{
	heap_init();
	util_init();
	hal_init(bc_audioboard);
	init_clock(1000, TIMER1);

#if 0
	gpio_make_output(GPIO_C5);
	gpio_set(GPIO_C5);
	gpio_clr(GPIO_C5);
	gpio_set(GPIO_C5);
	gpio_clr(GPIO_C5);
#endif

	hal_twi_init(0, NULL);

#define BAUD_8MHz_38461	12
	uart_init(RULOS_UART0, BAUD_8MHz_38461);

	memset(&aa_g, 0, sizeof(aa_g));
	aa_g.f = (ActivationFunc) sampleLocator;

	if (strlen(FIRMWARE_ID) > ID_OFFSET) {
		snprintf(aa_g.UARTsendBuf, sizeof(aa_g.UARTsendBuf)-1, "^i;%s\r\n", FIRMWARE_ID+ID_OFFSET);
	} else {
		snprintf(aa_g.UARTsendBuf, sizeof(aa_g.UARTsendBuf)-1, "^i;0$\r\n");
	}
	emit(&aa_g, aa_g.UARTsendBuf);

#ifdef TIME_DEBUG
	for (uint8_t i = 0; i < 10; i++)
		recordTime(precise_clock_time_us());
	printTimeRecords("res");
#endif

#if 0
	AliveAct alive;
	alive_init(&alive);
#endif

	pov_init(&aa_g.pov_act);
	message_change_init(&aa_g.message_change, &aa_g.pov_act);

	TiltyInputHandler tih;
	tih_init(&tih, &aa_g, &aa_g.pov_act, &aa_g.message_change);

	TiltyInputAct tia;
	tilty_input_init(&tia, &aa_g.accel, (UIEventHandler*) &tih);
	
	init_funcseq(&aa_g.funcseq, &aa_g, func_array);
	aa_g.debug_state = 'A';
	funcseq_next(&aa_g.funcseq);

	CpumonAct cpumon;
	cpumon_init(&cpumon);
	cpumon_main_loop();
	assert(FALSE);

	return 0;
}
