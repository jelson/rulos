//#define TIME_DEBUG
//#define NO_ACCEL
//#define NO_GYRO
#define NO_ULTRASOUND

#include <string.h>
#include <avr/interrupt.h>

#ifdef BOARD_REVC
#define F_CPU 8000000UL
#endif
#include <util/delay.h>

#define SAMPLING_PERIOD 50000
#define SCHED_QUANTUM   5000


#include "rocket.h"
#include "hardware.h"
#include "uart.h"
#include "twi.h"

struct locatorAct;
typedef struct locatorAct locatorAct_t;

typedef void (*PeripheralSendCompleteFunc)(locatorAct_t *aa);
typedef void (*PeripheralRecvCompleteFunc)(locatorAct_t *aa, char *data, int len);
void sampleLocator(locatorAct_t *locatorAct);

#ifndef NO_ULTRASOUND
static inline void chirp(uint16_t chirpLenUS);
#endif


typedef struct {
	uint16_t noiseMin, noiseMax;
	uint16_t threshMin, threshMax;
} usState_t;

#define US_TOP 0
#define US_BOT 1

struct locatorAct {
	// uart
	UartState_t uart;
	char UARTsendBuf[80];
	uint8_t uartSending;

	// twi
	char TWIsendBuf[10];
	char TWIrecvBuf[16];
	MediaRecvSlot *mrs;
	MediaStateIfc *twiState;
	PeripheralSendCompleteFunc sendFunc;
	PeripheralRecvCompleteFunc recvFunc;
	uint8_t twiAddr;
	uint8_t readLen;
	uint8_t debug;

#ifndef NO_ULTRASOUND
	// ultrasound
	char rangingMode;
	uint8_t trainingMode;
	uint8_t topOrBot;
	usState_t usState[2];
	uint16_t runLength;
	Time chirpSendTime;
	Time chirpRTT;
	uint8_t chirpTimeout;
#endif
};

static locatorAct_t locatorAct_g;


#define ACCEL_ADDR 0b0111000
#define ACCEL_SAMPLING_RATE_BITS 0b001 // 50 hz
#define ACCEL_RANGE_BITS 0b01 // +/- 4g

#define GYRO_ADDR  0b1101000
#define GYRO_FS_SEL 0x3 // 2000 deg/sec
#define GYRO_DLPF_CFG 0x3 // 42 hz

#define FIRMWARE_ID "$Rev$"
#define ID_OFFSET 6

#if defined(BOARD_REVB)
# define GPIO_10V_ENABLE GPIO_B0
# define GPIO_US_XMIT    GPIO_D6
# define US_TOP_CHAN     0
#elif defined(BOARD_REVC)
# define GPIO_10V_ENABLE     GPIO_B0
# define GPIO_US_XMIT        GPIO_D6
# define GPIO_US_XMIT_ENABLE GPIO_D2
# define GPIO_LED_1          GPIO_D4
# define GPIO_LED_2          GPIO_D3

# define US_TOP_CHAN         2
# define US_BOT_CHAN         3
#endif

#define US_QUIET_TRAINING_LEN 2000
#define US_MIN_NOISE_RANGE    25
#define US_MIN_RUN_LENGTH     5
#define US_CHIRP_LEN_US       1000
#define US_CHIRP_RING_TIME_MS 40

/****************************************************/


/***** serial port helpers ******/

// this is called at interrupt time
static inline void uartSendDone(void *cb_data)
{
	locatorAct_t *locatorAct = (locatorAct_t *) cb_data;
	locatorAct->uartSending = FALSE;
}

static inline void emit(locatorAct_t *locatorAct, char *s)
{
	uart_send(&locatorAct->uart, s, strlen(s), uartSendDone, locatorAct);
}

static inline uint8_t maybe_claim_serial(locatorAct_t *locatorAct)
{
	uint8_t retval = FALSE;
	uint8_t old_interrupts = hal_start_atomic();
	if (locatorAct->uartSending == FALSE) {
		locatorAct->uartSending = TRUE;
		retval = TRUE;
	}
	hal_end_atomic(old_interrupts);
	return retval;
}

// warning -- don't use from within interrupt context!
static inline void wait_for_serial(locatorAct_t *locatorAct)
{
	while (!maybe_claim_serial(locatorAct))
		_delay_us(100); // 32 microseconds per 8-bit character at 250kbit/sec
}



/**** time-related debugging ****/

#ifdef TIME_DEBUG

#define MAX_TIME_RECORDS 30
Time timerecord[MAX_TIME_RECORDS];
uint8_t numTimeRecords = 0;

void recordTime(Time t)
{
	if (numTimeRecords < MAX_TIME_RECORDS) {
		timerecord[numTimeRecords++] = t;
	}
}
void printTimeRecords(char *prefix)
{
	uint8_t i;

	for (i = 0; i < numTimeRecords; i++) {
		wait_for_serial(&locatorAct_g);
		snprintf(locatorAct_g.UARTsendBuf, sizeof(locatorAct_g.UARTsendBuf)-1,
				 "%s %d: %ld\r\n",
				 prefix, i, timerecord[i]);
		emit(&locatorAct_g, locatorAct_g.UARTsendBuf);
	}
}
#endif


/*** sending data to a locator peripheral ***/

void _peripheralSendComplete(void *user_data)
{
	locatorAct_t *locatorAct = (locatorAct_t *) user_data;
	locatorAct->sendFunc(locatorAct);
}

void sendToPeripheral(locatorAct_t *locatorAct,
		      uint8_t twiAddr,
		      char *data,
		      uint8_t len,
		      PeripheralSendCompleteFunc f)
{
	locatorAct->sendFunc = f;
	(locatorAct->twiState->send)(locatorAct->twiState, twiAddr, data, len, _peripheralSendComplete, locatorAct);
}


/*** receiving data from a locator peripheral, using the
	 write-address-then-read protocol ***/

void _locatorReadComplete(MediaRecvSlot *mrs, uint8_t len)
{
	locatorAct_t *locatorAct = (locatorAct_t *) mrs->user_data;
	locatorAct->recvFunc(locatorAct, mrs->data, locatorAct->readLen);
}

void _readFromPeripheral2(locatorAct_t *locatorAct)
{
	locatorAct->mrs = (MediaRecvSlot *) locatorAct->TWIrecvBuf;
	locatorAct->mrs->func = _locatorReadComplete;
	locatorAct->mrs->capacity = locatorAct->readLen;
	locatorAct->mrs->user_data = locatorAct;
	hal_twi_start_master_read((TwiState *) locatorAct->twiState, locatorAct->twiAddr, locatorAct->mrs);
}


void readFromPeripheral(locatorAct_t *locatorAct,
						uint8_t twiAddr,
						uint16_t baseAddr,
						int len,
						PeripheralRecvCompleteFunc f)
{
	locatorAct->recvFunc = f;
	locatorAct->readLen = len;
	assert(len + sizeof(MediaRecvSlot) < sizeof(locatorAct->TWIrecvBuf));

	locatorAct->twiAddr = twiAddr;
	locatorAct->TWIsendBuf[0] = baseAddr;
	sendToPeripheral(locatorAct, twiAddr, locatorAct->TWIsendBuf, 1, _readFromPeripheral2);

}


///////////// Ultrasound ////////////////////////////////////

#ifndef NO_ULTRASOUND

void start_sampling(locatorAct_t *locatorAct, uint8_t topOrBot)
{
	uint8_t adcChan;

	// for Rev C and after, put output driver into high impedance
#ifdef GPIO_US_XMIT_ENABLE
	gpio_clr(GPIO_US_XMIT_ENABLE);
#endif

	locatorAct->topOrBot = topOrBot;

	if (topOrBot == US_TOP)
		adcChan = US_TOP_CHAN;
#ifdef US_BOT_CHAN

	else if (topOrBot == US_BOT)
		adcChan = US_BOT_CHAN;
#endif
	else {
		emit(locatorAct, "invalid channel sent to start_sampling");
		return;
	}

	hal_init_adc_channel(adcChan);

	// At 8mhz, an ADC prescaler of 32 gives an ADC clock of 250 khz.
	// At 13 cycles per conversion, we're sampling at 19.2 khz.

	ADCSRA = 
		_BV(ADEN) | // enable ADC
		_BV(ADATE) | // auto-trigger mode enable
		_BV(ADIE)  | // enable interrupts
		_BV(ADPS2) | _BV(ADPS1) // prescaler: 64
		;

	// configure adc channel, and also set other bits in the register
	// to 0 (which uses aref, and sets adlar to 0)
	ADMUX = adcChan;

	// set to 'free running' mode
	ADCSRB = 0;

	// start conversions
	reg_set(&ADCSRA, ADSC);
}

void stop_sampling()
{
	ADCSRA = 0;
}

void updateNoiseThresholds(usState_t *usState, locatorAct_t *locatorAct)
{
	uint16_t range = usState->noiseMax - usState->noiseMin;

	// in case it happens to be very not-noisy when we train, enforce
	// a minimum noise band
	if (range < US_MIN_NOISE_RANGE)
		range = US_MIN_NOISE_RANGE;
	
	usState->threshMin = usState->noiseMin - (range/2);
	usState->threshMax = usState->noiseMax + (range/2);

	if (maybe_claim_serial(locatorAct)) { // can't use wait_for_serial; in interrupt context!
		snprintf(locatorAct->UARTsendBuf, sizeof(locatorAct->UARTsendBuf)-1,
				 "^t;side=%d;l=%5d;h=%5d$\r\n",
				 locatorAct->topOrBot,
				 usState->noiseMin,
				 usState->noiseMax);
		emit(locatorAct, locatorAct->UARTsendBuf);
	}
}

void trainUltrasound(uint16_t adcval, usState_t *usState, locatorAct_t *locatorAct)
{
	// If we're in training mode, see if the current ADC value is
	// lower or higher than the previously computed noise threshold.
	if (usState->noiseMin == 0 && usState->noiseMax == 0) {
		usState->noiseMin = usState->noiseMax = adcval;
		updateNoiseThresholds(usState, locatorAct);
	} else {
		if (adcval < usState->noiseMin) {
			usState->noiseMin = adcval;
			updateNoiseThresholds(usState, locatorAct);
		}
		if (adcval > usState->noiseMax) {
			usState->noiseMax = adcval;
			updateNoiseThresholds(usState, locatorAct);
		}
	}
}

void detectChirp(uint16_t adcval, usState_t *usState, locatorAct_t *locatorAct)
{
	// See if the current ADC value is beyond the noise threshold.
	// If so, increase the run length.  If not, set runlength to 0 and return.
	if (adcval >= usState->threshMin && adcval <= usState->threshMax) {
		locatorAct->runLength = 0;
		return;
	}

	// Increment the run length and see if it exceeds our threshold
	if (++locatorAct->runLength > US_MIN_RUN_LENGTH) {
		// US detected!

		// reset the run-length detector
		locatorAct->runLength = 0;

		// If we're in reflector mode, wait 500usec, then chirp back.
		if (locatorAct->rangingMode == 'r') {
			_delay_ms(US_CHIRP_RING_TIME_MS + 10);
			chirp(US_CHIRP_LEN_US);
			_delay_ms(US_CHIRP_RING_TIME_MS); // make sure we don't reply to our own ping
			if (maybe_claim_serial(locatorAct)) { // can't use wait_for_serial; in interrupt context!
				snprintf(locatorAct->UARTsendBuf, sizeof(locatorAct->UARTsendBuf)-1,
						 "^m;Sent chirp reply %ld$\n\r", precise_clock_time_us());
				emit(locatorAct, locatorAct->UARTsendBuf);
			}
		}

		// If we're in sender mode (waiting for a reflection), and we
		// got a chirp, record it and exit sender mode
		if (locatorAct->rangingMode == 's') {
			locatorAct->rangingMode = 'q';
			Time end = precise_clock_time_us();
			locatorAct->chirpRTT = end - locatorAct->chirpSendTime;
			// note, we don't just print the message here because we
			// wan this message to be reliable, meaning we have to
			// potentially wait until the serial port is free.  We
			// can't wait for the serial port to be free here in
			// interrupt context, so we store the chirpRTT and return.
			// The scheduled callback that runs in normal mode prints
			// the time.  The "sent chirp reply" message above isn't
			// critical, so we just send it here; if the serial port
			// is busy it'll be dropped, but that's not too bad.
		}
	}
}

// Called when the ADC has a sample ready for us.
ISR(ADC_vect)
{
 	uint16_t adcval = ADCL;
	adcval |= ((uint16_t) ADCH << 8);

	if (locatorAct_g.trainingMode) {
		// Are we in training mode?
		trainUltrasound(adcval, &locatorAct_g.usState[locatorAct_g.topOrBot], &locatorAct_g);
	} else {
		// Otherwise, detect
		detectChirp(adcval, &locatorAct_g.usState[locatorAct_g.topOrBot], &locatorAct_g);
	}
}


static inline void us_xmit_start()
{
#ifdef BOARD_REVB
	// We are using "fast pwm" mode, i.e., mode 7 (0b111), where TOP=OCR0A.

	// Furthermore since the on and off intervals are the same, we'll use 

	// atmega manual p.107: to have OC0A toggled on each compare match, set
	//    COM0A[1:0] = 0b01
	// running at 8mhz, getting a 40khz clock means switching every 200 cycles (prescaler=1)

	gpio_make_input(GPIO_US_XMIT);
	TCCR0A = _BV(COM0A0) | _BV(WGM01) | _BV(WGM00);
	TCCR0B = _BV(WGM02) | _BV(CS00);
	OCR0A = 100;
	TCNT0 = 0;
	gpio_make_output(GPIO_US_XMIT);
#elif BOARD_REVC
#else
# error "Don't know how to chrip on this board; sorry"
#endif
}

static inline void us_xmit_stop()
{
	TCCR0A = 0;
}

static inline void chirp(uint16_t chirpLenUS)
{
	us_xmit_start();
	_delay_us(chirpLenUS);
	us_xmit_stop();
}

#endif // NO_ULTRASOUND

/*************************************/

// all done!  start sampling.
void configLocator4(locatorAct_t *locatorAct)
{
#ifndef NO_ULTRASOUND
	locatorAct->trainingMode = 1;
	start_sampling(locatorAct, US_TOP);
	_delay_ms(400);
	stop_sampling();
# ifdef US_BOT_CHAN
	locatorAct->trainingMode = 1;
	start_sampling(locatorAct, US_BOT);
	_delay_ms(400);
	stop_sampling();
# endif
#endif
	wait_for_serial(locatorAct);
	emit(locatorAct, "^m;Initialization complete\n\r");

	schedule_us(1, (ActivationFuncPtr) sampleLocator, locatorAct);
}


// Next configure the gyro by writing the full-scale deflection and
// sample-rate bits to register 0x16.
void configLocator3(locatorAct_t *locatorAct)
{
#ifdef NO_GYRO
	// skip over gyro config if we have no gyro
	configLocator4(locatorAct);
	return;
#endif

	locatorAct->TWIsendBuf[0] = 0x16;
	locatorAct->TWIsendBuf[1] =
		(GYRO_FS_SEL << 3) |
		(GYRO_DLPF_CFG);
	sendToPeripheral(locatorAct, GYRO_ADDR, locatorAct->TWIsendBuf, 2, configLocator4);
}


// Configure the accelerometer by first reading config byte 0x14, then
// setting the low bits (leaving the 3 highest bits untouched), and
// writing it back.  This callback is called when the initial read
// completes.
void configLocator2(locatorAct_t *locatorAct, char *data, int len)
{
	locatorAct->TWIsendBuf[0] = 0x14;
	locatorAct->TWIsendBuf[1] =
		(data[0] & 0b11100000) |
		(ACCEL_RANGE_BITS << 3) |
		(ACCEL_SAMPLING_RATE_BITS);
	sendToPeripheral(locatorAct, ACCEL_ADDR, locatorAct->TWIsendBuf, 2, configLocator3);
}


void configLocator(locatorAct_t *locatorAct)
{
#ifndef NO_ULTRASOUND
	// turn off the us output switch
	gpio_make_output(GPIO_US_XMIT);
	gpio_clr(GPIO_US_XMIT);

	// enable 10v voltage doubler
	gpio_make_output(GPIO_10V_ENABLE);
	gpio_clr(GPIO_10V_ENABLE);
	//gpio_set(GPIO_10V_ENABLE);

	// for Rev C and after, put output driver into high impedance
#ifdef GPIO_US_XMIT_ENABLE
	gpio_make_output(GPIO_US_XMIT_ENABLE);
	gpio_clr(GPIO_US_XMIT_ENABLE);
#endif

	locatorAct->rangingMode = 'q';
	locatorAct->trainingMode = 0;
#endif

#ifdef BOARD_REVC
	gpio_make_output(GPIO_LED_1);
	gpio_make_output(GPIO_LED_2);
	gpio_clr(GPIO_LED_1);
	gpio_clr(GPIO_LED_2);
#endif

#ifdef NO_ACCEL
	configLocator3(locatorAct);
	return;
#endif

	readFromPeripheral(locatorAct, ACCEL_ADDR, 0x14, 1, configLocator2);
}


/*************************************/

static inline int16_t convert_accel(char lsb, char msb)
{
	int16_t mag = ((((int16_t) msb) & 0b01111111) << 2) | ((lsb & 0b11000000) >> 6);
	
	if (msb & 0b10000000)
		mag = mag-512;

	return mag;
}

#ifndef NO_GYRO
void sampleLocator3(locatorAct_t *locatorAct, char *data, int len)
{
	// convert and send the data out of serial
	int16_t x = ((int16_t) data[0]) << 8 | data[1];
	int16_t y = ((int16_t) data[2]) << 8 | data[3];
	int16_t z = ((int16_t) data[4]) << 8 | data[5];

	wait_for_serial(locatorAct);
	snprintf(locatorAct->UARTsendBuf, sizeof(locatorAct->UARTsendBuf)-1, "^g;x=%5d;y=%5d;z=%5d$\r\n", x, y, z);
	emit(locatorAct, locatorAct->UARTsendBuf);
}
#endif


void sampleLocator2(locatorAct_t *locatorAct, char *data, int len)
{
#ifndef NO_GYRO
	// read 6 bytes from gyro over TWI starting from address 0x2
	readFromPeripheral(locatorAct, GYRO_ADDR, 0x1D, 6, sampleLocator3);
#endif

#ifndef NO_ACCEL
	// convert and send the accelerometer data we just received out to
	// the serial port
	int16_t x = convert_accel(data[0], data[1]);
	int16_t y = convert_accel(data[2], data[3]);
	int16_t z = convert_accel(data[4], data[5]);

	if (maybe_claim_serial(locatorAct)) {
		snprintf(locatorAct->UARTsendBuf, sizeof(locatorAct->UARTsendBuf)-1, "^a;x=%5d;y=%5d;z=%5d$\r\n", x, y, z);
		emit(locatorAct, locatorAct->UARTsendBuf);
	}
#endif
}

void process_serial_command(locatorAct_t *locatorAct, char cmd)
{
	switch (cmd) {
#ifndef NO_ULTRASOUND
	case 'q':
		locatorAct->rangingMode = 'q';
		us_xmit_stop();

		wait_for_serial(locatorAct);
		emit(locatorAct, "^m;entering quiet mode$\n\r");
		break;

	case 'c':
		us_xmit_start();
		wait_for_serial(locatorAct);
		emit(locatorAct, "^m;transmitting continuous ultrasound$\n\r");
		break;

	case 's':
		locatorAct->rangingMode = 'q'; // keep interrupt-driven analog loop from replying,
                                       // if we were in reply mode
		wait_for_serial(locatorAct);
		emit(locatorAct, "^m;chirping$\n\r");
		chirp(US_CHIRP_LEN_US);
		_delay_ms(US_CHIRP_RING_TIME_MS);
		locatorAct->chirpSendTime = precise_clock_time_us();
		locatorAct->chirpTimeout = 0;
		locatorAct->chirpRTT = 0;
		locatorAct->rangingMode = 's';
		break;
			
	case 'r':
		locatorAct->rangingMode = 'r';
		wait_for_serial(locatorAct);
		emit(locatorAct, "^m;entering reflector mode$\n\r");
		break;
#endif
	}
}
	
	
void sampleLocator(locatorAct_t *locatorAct)
{
	// schedule reading of the next sample
	schedule_us(SAMPLING_PERIOD, (ActivationFuncPtr) sampleLocator, locatorAct);

	// set debug leds
#ifndef NO_ULTRASOUND
	if (locatorAct->rangingMode == 'r') {
		gpio_set(GPIO_LED_1);
	} else {
		locatorAct->debug++;
		if (locatorAct->debug == (1000000 / SAMPLING_PERIOD)) {
			gpio_set(GPIO_LED_1);
		} else if (locatorAct->debug == 2 * (1000000 / SAMPLING_PERIOD)) {
			gpio_clr(GPIO_LED_1);
			locatorAct->debug = 0;
		}
	}
#endif

#ifdef TEST_TIME_ONLY
	snprintf(locatorAct->UARTsendBuf, sizeof(locatorAct->UARTsendBuf)-1, "%d\r\n", locatorAct->debug);
	emit(locatorAct, locatorAct->UARTsendBuf);
	return;
#endif

	// Check for serial commands
	char cmd;
	while (CharQueue_pop(locatorAct->uart.recvQueue.q, &cmd))
		process_serial_command(locatorAct, cmd);

#ifndef NO_ULTRASOUND
	// If we're in 's' mode waiting for a reply, don't sample the
	// peripherals.  Just check for a timeout and then end.  This must
	// be done in an atomic block because the interrupt-driven analog
	// side may simultaneously be frobbing rangingMode, if it detected
	// a chirp just now.
	uint8_t timeout = FALSE;
	uint8_t listenMode = FALSE;
	uint8_t old_interrupts = hal_start_atomic();
	if (locatorAct->rangingMode == 's') {
		listenMode = TRUE;
		locatorAct->chirpTimeout++;

		if (locatorAct->chirpTimeout >= 4) {
			locatorAct->rangingMode = 'q';
			timeout = TRUE;
		}
	}
	hal_end_atomic(old_interrupts);

	if (listenMode) {
		if (timeout) {
			wait_for_serial(locatorAct); // note, must happen outside atomic block
			emit(locatorAct, "^m;ranging timeout$\n\r");
		} else {
			return; // don't sample peripherals if we're still listening.
		}
	}

	// If we were recently in 's' mode and sucessfully completed a
	// ranging trial, print the result
	if (locatorAct->chirpRTT) {
		wait_for_serial(locatorAct);
		snprintf(locatorAct->UARTsendBuf, sizeof(locatorAct->UARTsendBuf)-1,
				 "^d;t=%ld$\r\n",
				 locatorAct->chirpRTT);
		locatorAct->chirpRTT = 0;
		emit(locatorAct, locatorAct->UARTsendBuf);
	}
#endif // NO_ULTRASOUND

#ifdef NO_ACCEL
	sampleLocator2(locatorAct, NULL, 0);
	return;
#endif

	// read 6 bytes from accelerometer starting from address 0x2
	readFromPeripheral(locatorAct, ACCEL_ADDR, 0x2, 6, sampleLocator2);
}




int main()
{
	hal_init();
	init_clock(SCHED_QUANTUM, TIMER1);

	memset(&locatorAct_g, 0, sizeof(locatorAct_g));
	locatorAct_g.twiState = hal_twi_init(100, 0, NULL);
	uart_init(&locatorAct_g.uart, 100000, 1);

	if (strlen(FIRMWARE_ID) > ID_OFFSET) {
		snprintf(locatorAct_g.UARTsendBuf, sizeof(locatorAct_g.UARTsendBuf)-1, "^i;%s\r\n", FIRMWARE_ID+ID_OFFSET);
	} else {
		snprintf(locatorAct_g.UARTsendBuf, sizeof(locatorAct_g.UARTsendBuf)-1, "^i;0$\r\n");
	}
	wait_for_serial(&locatorAct_g);
	emit(&locatorAct_g, locatorAct_g.UARTsendBuf);

#ifdef TIME_DEBUG
	for (uint8_t i = 0; i < 10; i++)
		recordTime(precise_clock_time_us());
	printTimeRecords("res");
#endif

	configLocator(&locatorAct_g);

	CpumonAct cpumon;
	cpumon_init(&cpumon);
	cpumon_main_loop();
	assert(FALSE);

	return 0;
}
