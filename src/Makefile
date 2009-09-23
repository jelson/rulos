HDRS = rocket.h display_controller.h clock.h heap.h util.h display_rtc.h display_scroll_msg.h random.h display_compass.h board_buffer.h focus.h queue.h input_controller.h
SRCS = rocket.c sim.c display_controller.c clock.c heap.c display_rtc.c display_scroll_msg.c util.c random.c display_compass.c board_buffer.c focus.c queue.c input_controller.c

rocketsim: $(HDRS) $(SRCS)
	gcc -Wall -g -o rocketsim -DSIM $(SRCS) -lncurses
