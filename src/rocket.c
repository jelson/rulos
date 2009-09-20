#include <inttypes.h>

#include "rocket.h"
#include "clock.h"
#include "util.h"
#include "display_controller.h"
#include "display_rtc.h"
#include "display_scroll_msg.h"



/************************************************************************************/
/************************************************************************************/


int main()
{
	init_util();
#ifdef SIM
	sim_init();
#else
	hw_init();
#endif

	program_string(0, "init brd");
	clock_init();
	//install_handler(ADC, adc_handler);
	drtc_init();

	DScrollMsgAct da1;
	dscrlmsg_init(&da1, 2, "Hi jelson. Can you dig it?  ", 130);

	DScrollMsgAct da2;
	char buf[129-32];
	{
		int i;
		for (i=0; i<128-32; i++)
		{
			buf[i] = i+32;
		}
		buf[i] = '\0';
	}
	dscrlmsg_init(&da2, 3, buf, 75);

#ifdef SIM
	sim_run();
#else
	hw_run();
#endif

	return 0;
}

