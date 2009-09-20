#include <inttypes.h>

#ifdef SIM
# include <stdio.h>
# include <unistd.h>
#endif

#include "rocket.h"
#include "clock.h"
#include "display_rtc.h"
#include "display_controller.h"



/************************************************************************************/
/************************************************************************************/


int main()
{
#ifdef SIM
	init_sim();
#else
	init_hardware();
#endif

	clock_init();
	//install_handler(ADC, adc_handler);
	drtc_init();


#ifdef SIM
	while (1) {
		sleep(1);
	}
#endif

	return 0;
}

