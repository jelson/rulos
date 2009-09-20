HDRS = rocket.h display_controller.h clock.h heap.h util.h display_rtc.h display_scroll_msg.h
SRCS = rocket.c sim.c display_controller.c clock.c heap.c display_rtc.c display_scroll_msg.c util.c

rocketsim: $(HDRS) $(SRCS)
	gcc -Wall -g -o rocketsim -DSIM $(SRCS) -lncurses
