#include "rocket.h"
#include "queue.h"

QUEUE_DECLARE(short)

#include "queue.mc"
QUEUE_DEFINE(short)

#define NUM_ELTS 4

int main()
{
	util_init();

	uint8_t space[sizeof(shortQueue)+sizeof(short)*NUM_ELTS];
	shortQueue *sq = (shortQueue*) space;
	shortQueue_init(sq, sizeof(space));

	short in = 0, out = 0;

	while (TRUE)
	{
		if (deadbeef_rand() & 1)
		{
			r_bool rc = shortQueue_append(sq, in);
			if (rc)
			{
				in++;
				LOGF((logfp, "in: %d\n", in));
			}
			else
			{
				LOGF((logfp, "full\n"));
			}
		}
		else
		{
			short val;
			r_bool rc = shortQueue_pop(sq, &val);
			if (rc) {
				assert(val==out);
				LOGF((logfp, "        out: %d\n", out));
				out++;
			}
			else
			{
				LOGF((logfp, "empty\n"));
			}
		}
	}
}
