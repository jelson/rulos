HDRS = rocket.h display_controller.h clock.h
SRCS = rocket.c sim.c display_controller.c clock.c

rocketsim: $(HDRS) $(SRCS)
	gcc -Wall -o rocketsim -DSIM $(SRCS) -lncurses
