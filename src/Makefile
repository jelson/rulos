HDRS = rocket.h display_controller.h clock.h heap.h util.h display_rtc.h
SRCS = rocket.c sim.c display_controller.c clock.c heap.c display_rtc.c

rocketsim: $(HDRS) $(SRCS)
	gcc -Wall -o rocketsim -DSIM $(SRCS) -lncurses
