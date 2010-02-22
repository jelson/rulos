#ifndef __hal_h__
#define __hal_h__

#ifndef __rocket_h__
# error Please include rocket.h instead of this file
#endif

/*
 * Functions that are separately implemented by both the simulator and the real hardware
 */

typedef void (*Handler)();

typedef enum {
	bc_rocket0,
	bc_rocket1,
	bc_audioboard,
	bc_wallclock,
	bc_chaseclock,
} BoardConfiguration;

void hal_init(BoardConfiguration bc);

void hal_start_atomic();	// block interrupts/signals
void hal_end_atomic();		// resume interrupts/signals
void hal_idle();			// hw: spin. sim: sleep

#define TIMER1	(1)
#define TIMER2	(2)
uint32_t hal_start_clock_us(uint32_t us, Handler handler, uint8_t timer_id);

void hal_program_segment(uint8_t board, uint8_t digit, uint8_t segment, uint8_t onoff);
char hal_read_keybuf();
uint16_t hal_elapsed_milliintervals();
void hal_speedup_clock_ppm(int32_t ratio);
void hal_delay_ms(uint16_t ms);

void hal_init_keypad();
void hal_init_adc(Time scan_period);
void hal_init_adc_channel(uint8_t idx);
uint16_t hal_read_adc(uint8_t idx);

void hal_init_joystick_button();
r_bool hal_read_joystick_button();

void hal_uart_init(uint16_t baud);
/*
void hal_uart_send_byte(uint8_t byte);
void hal_uart_set_recv_handler(Activation *act);
	// Runs on interrupt stack -- keep it sweet and race-free.
*/

/////////////// TWI ///////////////////////////////////////////////

typedef uint8_t Addr;

struct s_TWIRecvSlot;
typedef void (*TWIRecvDoneFunc)(struct s_TWIRecvSlot *recvSlot, uint8_t len);
typedef struct s_TWIRecvSlot {
	TWIRecvDoneFunc func;
	uint8_t capacity;
	uint8_t occupied;
	void *user_data; // storage for a pointer back to your state structure
	char data[0];
} TWIRecvSlot;

void hal_twi_init(Addr local_addr, TWIRecvSlot *trs);

typedef void (*TWISendDoneFunc)(void *user_data);
void hal_twi_send(Addr dest_addr, char *data, uint8_t len, 
				  TWISendDoneFunc sendDoneCB, void *sendDoneCBData);

////////////////// Audio ////////////////////////////////////////////

RingBuffer *hal_audio_init(uint16_t sample_period_us);

struct s_hal_spi_ifc;
typedef void (*HALSPIFunc)(struct s_hal_spi_ifc *ifc, uint8_t data);
typedef struct s_hal_spi_ifc {
	HALSPIFunc func;
} HALSPIIfc;

#define SPIFLASH_CMD_READ_DATA	(0x03)

void hal_init_spi(HALSPIIfc *spi);
void hal_spi_open();
void hal_spi_send(uint8_t byte);
void hal_spi_close();

#endif // __hal_h__
