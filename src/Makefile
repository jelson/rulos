HDRS = rocket.h display_controller.h clock.h heap.h util.h display_rtc.h display_scroll_msg.h random.h display_compass.h board_buffer.h focus.h queue.h input_controller.h cursor.h labeled_display.h display_docking.h drift_anim.h display_gratuitous_graph.h region.h numeric_input.h
SRCS = rocket.c sim.c display_controller.c clock.c heap.c display_rtc.c display_scroll_msg.c util.c random.c display_compass.c board_buffer.c focus.c queue.c input_controller.c cursor.c labeled_display.c display_docking.c drift_anim.c display_gratuitous_graph.c numeric_input.c

rocketsim: $(HDRS) $(SRCS)
	gcc -Wall -g -o rocketsim -DSIM $(SRCS) -lncurses
