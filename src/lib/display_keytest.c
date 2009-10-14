
#include "rocket.h"
#include "display_keytest.h"

static void update(KeyTestActivation_t *kta)
{
	schedule_us(50000, (Activation *) kta);
	char k = hal_read_keybuf();

	LOGF((logfp, "in update for keytest\n"))
	if (!k)
		return;

	LOGF((logfp, "got char %c\n", k))

	memmove(kta->bbuf.buffer, kta->bbuf.buffer+1, NUM_DIGITS-1);
	kta->bbuf.buffer[NUM_DIGITS-1] = ascii_to_bitmap(k);
	board_buffer_draw(&kta->bbuf);

}

void display_keytest_init(KeyTestActivation_t *kta, uint8_t board)
{
	kta->f = (ActivationFunc) update;
	board_buffer_init(&kta->bbuf);
	board_buffer_push(&kta->bbuf, board);
	schedule_us(1, (Activation *) kta);
}

