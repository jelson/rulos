
/*-----------------------------------------------------*\
|  USI I2C Slave Driver                                 |
|                                                       |
| This library provides a robust, interrupt-driven I2C  |
| slave implementation built on the ATTiny Universal    |
| Serial Interface (USI) hardware.  Slave operation is  |
| implemented as a register bank, where each 'register' |
| is a pointer to an 8-bit variable in the main code.   |
| This was chosen to make I2C integration transparent   |
| to the mainline code and making I2C reads simple.     |
| This library also works well with the Linux I2C-Tools |
| utilities i2cdetect, i2cget, i2cset, and i2cdump.     |
|                                                       |
| Adam Honse (GitHub: CalcProgrammer1) - 7/29/2012      |
|            -calcprogrammer1@gmail.com                 |
\*-----------------------------------------------------*/

#include <avr/interrupt.h>
#include <avr/io.h>

#include "chip/avr/core/usi_pins.h"
#include "chip/avr/periph/usi_twi_slave/usi_twi_slave.h"
#include "core/rulos.h"
#include "hardware.h"

// Debug

//#define ENABLE_DEBUG_STROBE

#ifdef ENABLE_DEBUG_STROBE

#define DEBUG_STROBE_GPIO GPIO_A5

void debug_strobe_init() {
  gpio_make_output(DEBUG_STROBE_GPIO);
  gpio_set(DEBUG_STROBE_GPIO);
}

void debug_strobe(uint8_t count) {
  while (count-- > 0) {
    gpio_set(DEBUG_STROBE_GPIO);
    gpio_clr(DEBUG_STROBE_GPIO);
  }
}

#define DEBUG_STROBE_INIT() debug_strobe_init()
#define DEBUG_STROBE(count) debug_strobe(count)

#else  // ENABLE_DEBUG_STROBE

#define DEBUG_STROBE_INIT()
#define DEBUG_STROBE(count)

#endif

typedef enum {
  USI_SLAVE_CHECK_ADDRESS,
  USI_SLAVE_SEND_DATA,
  USI_SLAVE_REQUEST_REPLY_FROM_SEND_DATA,
  USI_SLAVE_CHECK_REPLY_FROM_SEND_DATA,
  USI_SLAVE_RECV_DATA_WAIT,
  USI_SLAVE_RECV_DATA_ACK_SEND
} UsiTwiSlaveStates;

typedef struct {
  uint8_t my_address;
  MediaRecvSlot* recv_slot;
  int8_t recv_len;  // must be signed since we use -1 as a sentinel
  usi_slave_send_func send_func;
  UsiTwiSlaveStates state;
} UsiTwiSlaveContext;

static UsiTwiSlaveContext usi;

// On each clock, the USI counter register increments by two; one for each clock
// edge. (Note data is shifted on the positive edge.) Since it's a 4-bit counter
// that overflows during the transition from 15 back to 0, we set it to 14 if
// we want an overflow interrupt after 1 bit transferred (two clocks to overflow),
// or zero if we want 8 bits (16 clocks to overflow).
#define USI_WANT_1_BIT (0xe)
#define USI_WANT_8_BITS (0)

static void usi_poll_for_stop(void *data);

////////////////////////////////////////////////////////////////////////////////////////////////////

// Configures the TWI bus when it's idle, waiting for a start condition.
static void usi_twi_slave_idle_bus()
{
  USICR =
    (1 << USISIE) |  // enable start condition interrupt
    (0 << USIOIE) |  // !enable overflow interrupt
    (1 << USIWM1) |  // set usi in two-wire mode, disable bit counter overflow hold
    (0 << USIWM0) |
    (1 << USICS1) |  // shift register clock source = external, positive edge,
    (0 << USICS0) |  // 4-bit counter source = external, both edges
    (0 << USICLK) |  
    (0 << USITC);    // don't toggle clock-port pin

  USISR =
    (0 << USISIF) |  // don't clear start condition flag
    (1 << USIOIF) |  // clear overflow condition flag
    (1 << USIPF) |   // clear stop condition flag
    (1 << USIDC);    // clear arbitration error flag
}

/////////////////////////////////////////////////////////////////////////////////
// ISR USI_START_vect - USI Start Condition Detector Interrupt                 //
//                                                                             //
//  This interrupt occurs when the USI Start Condition Detector detects a      //
//  start condition.  A start condition marks the beginning of an I2C          //
//  transmission and occurs when SDA has a high->low transition followed by an //
//  SCL high->low transition.  When a start condition occurs, the I2C slave    //
//  state is set to check address mode and the counter is set to wait 8 clocks //
//  (enough for the address/rw byte to be transmitted) before overflowing and  //
//  triggering the first state table interrupt.  If a stop condition occurs,   //
//  reset the start condition detector to detect the next start condition.     //
/////////////////////////////////////////////////////////////////////////////////
ISR(USI_START_VECT)
{
  DEBUG_STROBE(1);
  
  usi.state = USI_SLAVE_CHECK_ADDRESS;

  // Set SDA as input
  gpio_make_input(USI_SDA);

  // wait for SCL to go low to ensure the Start Condition has completed (the
  // start detector will hold SCL low). If a Stop Condition arises then leave
  // the interrupt to prevent waiting forever.
  //
  // The author of another USI slave driver writes a comment here:
  // "Don't use USISR to test for Stop Condition as in Application Note AVR312 because
  // the Stop Condition Flag is going to be set from the last TWI sequence."
  // I'm not sure I agree with this; the USI TWI SLave applicatoin note does not check
  // USISR here.
  while (gpio_is_set(USI_SCL) && gpio_is_clr(USI_SDA)) {
    // wait
  };

  USICR =
    (1 << USISIE) |  // enable start condition interrupt
    (1 << USIOIE) |  // enable overflow interrupt
    (1 << USIWM1) |  // set usi in two-wire mode, enable bit counter overflow hold
    (1 << USIWM0) |
    (1 << USICS1) |  // shift register clock source = external, positive edge,
    (0 << USICS0) |  // 4-bit counter source = external, both edges
    (0 << USICLK) |  
    (0 << USITC);    // don't toggle clock-port pin

  USISR =
    (1 << USISIF) |  // clear start condition flag
    (1 << USIOIF) |  // clear overflow condition flag
    (1 << USIPF)  |  // clear stop condition flag
    (1 << USIDC)  |  // clear arbitration error flag
    (USI_WANT_8_BITS << USICNT0); // get an interrupt after bits (the address)

  DEBUG_STROBE(1);
}

// Used to clear the overflow interrupt and set the 4-bit counter to
// wait for either 1 or 8 clocks.
static void usi_twi_slave_ack_overflow(uint8_t num_bits_to_recv)
{
  USISR =
    (0 << USISIF) |  // don't clear start condition flag
    (1 << USIOIF) |  // clear overflow condition flag
    (1 << USIPF)  |  // clear stop condition flag
    (1 << USIDC)  |  // clear arbitration error flag
    (num_bits_to_recv << USICNT0); // set counter to 8 or 1 bits
}

/////////////////////////////////////////////////////////////////////////////////
// ISR USI_OVERFLOW_vect - USI Overflow Interrupt                              //
//                                                                             //
//  This interrupt occurs when the USI counter overflows.  By setting this     //
//  counter to 8, the USI can be commanded to wait one byte length before      //
//  causing another interrupt (and thus state change).  To wait for an ACK,    //
//  set the counter to 1 (actually -1, or 0x0E) it will wait one clock.        //
//  This is used to set up a state table of I2C transmission states that fits  //
//  the I2C protocol for proper transmission.                                  //
/////////////////////////////////////////////////////////////////////////////////
ISR(USI_OVF_VECT)
{
  DEBUG_STROBE(2);
  
  switch (usi.state) {

    // The first state after the start condition, this state checks the
    // received byte against the stored slave address as well as the
    // global transmission address of 0x00.  If there is a match, the R/W
    // bit is checked to branch either to sending or receiving modes.
    // If the address was not for this device, the USI system is
    // re-initialized for start condition.
  case USI_SLAVE_CHECK_ADDRESS: {
    uint8_t data = USIDR;
    uint8_t direction = data & 0x1;
    uint8_t address = data >> 1;

    DEBUG_STROBE(3);
    
    // If this packet wasn't meant for us, or the broadcast (address 0),
    // reset the bus and wait for the next start.
    if (address != 0 && address != usi.my_address) {
      DEBUG_STROBE(4);
      usi_twi_slave_idle_bus();
      return;
    }

    if (direction) {
      // Slave-send mode. Send an ACK.
      DEBUG_STROBE(5);
      usi.state = USI_SLAVE_SEND_DATA;
      USIDR = 0;
    } else {
      // Slave-receive mode. Send an ack only if we have buffer space
      // available; otherwise, NACK.
      DEBUG_STROBE(5);
      if (usi.recv_slot != NULL && usi.recv_slot->occupied_len == 0 && usi.recv_len == -1) {
        schedule_now(usi_poll_for_stop, NULL);
        usi.recv_len = 0;
        USIDR = 0;
      } else {
        USIDR = 1;
      }
      usi.state = USI_SLAVE_RECV_DATA_WAIT;
    }

    gpio_make_output(USI_SDA);
    usi_twi_slave_ack_overflow(USI_WANT_1_BIT);
    break;
  }

    //////// Master-Transmitter States /////////////
    
    //  Prepares to wait 8 clocks to receive a data byte from the master.
  case USI_SLAVE_RECV_DATA_WAIT: {
    usi.state = USI_SLAVE_RECV_DATA_ACK_SEND;
    gpio_make_input(USI_SDA);
    usi_twi_slave_ack_overflow(USI_WANT_8_BITS);
    break;
  }

    // A byte has been received from the master. If we have room in our local
    // receive buffer, append it and ACK the byte. Otherwise, NACK the byte
    // and abort the reception.
  case USI_SLAVE_RECV_DATA_ACK_SEND: {
    if (usi.recv_len < 0) {
      // If we've already experienced an overflow on this packet, NACK the byte.
      USIDR = 1;
    } else if (usi.recv_slot->capacity <= usi.recv_len) {
      // If the receive buffer just overflowed, record the overflow and NACK the byte.
      usi.recv_len = -1;
      USIDR = 1;
    } else {
      // Store and ACK the byte.
      usi.recv_slot->data[usi.recv_len++] = USIDR;
      USIDR = 0;
    }
    
    usi.state = USI_SLAVE_RECV_DATA_WAIT;
    gpio_make_output(USI_SDA);
    usi_twi_slave_ack_overflow(USI_WANT_1_BIT);
    break;
  }

    //////// Master-Receiver States /////////////

    // Wait 1 clock period for the master to ACK or NACK the sent data
    // If master NACKs, it means that master doesn't want any more data.
  case USI_SLAVE_REQUEST_REPLY_FROM_SEND_DATA: {
    //After sending, set SDA as input.
    gpio_make_input(USI_SDA);
    USIDR = 0;
    usi_twi_slave_ack_overflow(USI_WANT_1_BIT);
    usi.state = USI_SLAVE_CHECK_REPLY_FROM_SEND_DATA;
    break;
  }

    // Check USIDR to see if master sent ACK or NACK.  If NACK, set up
    // a reset to START conditions, if ACK, fall through into SEND_DATA
    // to continue sending data.
  case USI_SLAVE_CHECK_REPLY_FROM_SEND_DATA: {
    if (USIDR) {
      // The master sent a NACK, indicating that it will not accept
      // more data.  Reset into START condition state.
      usi_twi_slave_idle_bus();
      return;
    }

    // If the master ACKs, fall through into SEND_DATA
  }

    // Make an upcall to determine the data to be sent and set it to USIDR.
    // Set status register to wait 8 clocks and wait for ACK or NACK.
  case USI_SLAVE_SEND_DATA: {
    if (usi.send_func != NULL) {
      USIDR = usi.send_func();
    } else {
      USIDR = 0xCD;  // a constant to indicate something's wrong
    }
    gpio_make_output(USI_SDA);
    usi.state = USI_SLAVE_REQUEST_REPLY_FROM_SEND_DATA;
    usi_twi_slave_ack_overflow(USI_WANT_8_BITS);
    break;
  }
  }
}

// Do you want to know what's stupid? The USI module in attiny hardware is stupid.
// There's no way to detect a stop condition other than to poll for it. Amazing.
// We start this "userspace" poller when we get the start of an incoming packet
// and don't exit until we get a STOP.
static void usi_poll_for_stop(void *data)
{
  while (!(USISR & _BV(USIPF))) {
    DEBUG_STROBE(7);
  }

  // clear the stop condition flag
  USISR |= _BV(USIPF);

  // If a packet was received, call the user's callback.
  if (usi.recv_len > 0 && usi.recv_slot->func != NULL) {
    DEBUG_STROBE(8);
    usi.recv_slot->occupied_len = usi.recv_len;
    usi.recv_slot->func(usi.recv_slot, usi.recv_len);
    DEBUG_STROBE(8);
  }
  usi.recv_len = -1;
  usi_twi_slave_idle_bus();
}

void usi_twi_slave_init(char address, MediaRecvSlot* recv_slot, usi_slave_send_func send_func)
{
  DEBUG_STROBE_INIT();
    
  usi.my_address = address;
  usi.recv_slot = recv_slot;
  usi.recv_len = -1;
  usi.send_func = send_func;
  usi.state = USI_SLAVE_CHECK_ADDRESS;

  gpio_set(USI_SCL);
  gpio_set(USI_SDA);
  gpio_make_output(USI_SCL);
  gpio_make_input(USI_SDA);
  
  usi_twi_slave_idle_bus();

  // enable interrupts, in case nothing else in the program has done so already
  sei();
}
