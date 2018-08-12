#include <stdio.h>
#include <stdlib.h>
#include <inttypes.h>
#include <gtk/gtk.h>

#include "core/rulos.h"

#define X_LEDS 24
#define Y_LEDS 16
#define LED_RADIUS 20
#define LED_SPACING 8

#define X_SIZE (X_LEDS*(LED_RADIUS + LED_SPACING))
#define Y_SIZE (Y_LEDS*(LED_RADIUS + LED_SPACING))

static GtkWidget *topWindow = NULL;
static GtkWidget *drawing_area = NULL;
static GdkPixmap *canvas = NULL;
static GdkGC *canvas_gc = NULL;
static int ready;

void destroy (void)
{
	gtk_main_quit();
	exit(1);
}

/* Redraw the screen from the backing pixmap */
static gint expose_topWindow(GtkWidget *widget, GdkEventExpose *event)
{
	gdk_draw_pixmap(widget->window,
					widget->style->fg_gc[GTK_WIDGET_STATE (widget)],
					canvas,
					event->area.x, event->area.y,
					event->area.x, event->area.y,
					event->area.width, event->area.height);
	
	return FALSE;
}

static void drawLed(int x, int y, int rColor, int gColor)
{
	GdkColor ledColor = { 0, (uint8_t) (rColor * (0xffff/16)), (uint8_t) (gColor * (0xffff/16)), 0 };
	GdkColor ledOutline = { 0, 0x4000, 0x4000, 0x4000 };
	int xCoord = LED_SPACING/2 + x*(LED_SPACING+LED_RADIUS);
	int yCoord = LED_SPACING/2 + y*(LED_SPACING+LED_RADIUS);

	gdk_gc_set_rgb_fg_color(canvas_gc, &ledColor);

	gdk_draw_arc(canvas,
				 canvas_gc,
				 TRUE,
				 xCoord, yCoord,
				 LED_RADIUS, LED_RADIUS,
				 0, 360*64);
	
	gdk_gc_set_rgb_fg_color(canvas_gc, &ledOutline);
	
	gdk_draw_arc(canvas,
				 canvas_gc,
				 FALSE,
				 xCoord, yCoord,
				 LED_RADIUS, LED_RADIUS,
				 0, 360*64);
}

static void initCanvas()
{
	canvas = gdk_pixmap_new(drawing_area->window,
							X_SIZE,
							Y_SIZE,
							-1);

	canvas_gc = gdk_gc_new(canvas);
	GdkColor black = { 0, 0, 0, 0};
	gdk_gc_set_rgb_fg_color(canvas_gc, &black);
	gdk_draw_rectangle(canvas,
					   canvas_gc,
					   TRUE,
					   0, 0,
					   topWindow->allocation.width,
					   topWindow->allocation.height);
	
	int x, y;
	for (y = 0; y < Y_LEDS; y++)
		for (x = 0; x < X_LEDS; x++)
			drawLed(x, y, 0, 0);
}


/****************************/

static void start_gui()
{
	gtk_init(NULL, NULL);

	topWindow = gtk_window_new(GTK_WINDOW_TOPLEVEL);

	gtk_window_set_default_size(GTK_WINDOW(topWindow),
								X_SIZE,
								Y_SIZE);

	gtk_signal_connect(GTK_OBJECT(topWindow), "destroy",
					   GTK_SIGNAL_FUNC(destroy), NULL);

	drawing_area = gtk_drawing_area_new();
	gtk_signal_connect(GTK_OBJECT(drawing_area), "expose_event",
					   GTK_SIGNAL_FUNC(expose_topWindow), NULL);
	gtk_widget_show(topWindow);

	gtk_container_add(GTK_CONTAINER(topWindow), drawing_area);
	gtk_widget_show(drawing_area);

	initCanvas();
	ready = TRUE;
	gdk_threads_enter();
	gtk_main();
	gdk_threads_leave();
}

void hal_6matrix_init(SixMatrix_Context_t *mat)
{
	// jonh removed this call; it's now deprecated
	//g_thread_init(NULL);
	gdk_threads_init();
	ready = FALSE;
	// jonh changed deprecated g_thread_create to g_thread_new
	//g_thread_create((GThreadFunc) start_gui, NULL, FALSE, NULL);
	g_thread_new("gui", (GThreadFunc) start_gui, NULL);
	while (!ready) {
		usleep(10000);
	}
}


static void sim_printrow(uint8_t *colBytes, uint8_t numBytes, uint8_t rowNum)
{
	return;
	printf("row %d:", rowNum);

	int i = 0;
	for (i = 0; i < numBytes; i++) {
		printf(" 0o%04o", colBytes[i]);
	}
	printf("\n");
}

void hal_6matrix_setRow_2bit(SixMatrix_Context_t *mat, uint8_t *colBytes, uint8_t rowNum)
{
	sim_printrow(colBytes, SIXMATRIX_NUM_COL_BYTES_2BIT, rowNum);

	gdk_threads_enter();

	int byteNum;
	int colNum = 0;

	for (byteNum = 0; byteNum < SIXMATRIX_NUM_COL_BYTES_2BIT; byteNum++, colBytes++) {
		uint8_t currByte = *colBytes;
		int i;

		for (i = 0; i < 4; i++) {
			drawLed(
					colNum,
					rowNum,
					(currByte & 0b10000000) ? 16 : 0,
					(currByte & 0b01000000) ? 16 : 0
					);
			colNum++;
			currByte <<= 2;
		}
	}
	gtk_widget_draw(drawing_area, NULL);
	gdk_threads_leave();
}

void hal_6matrix_setRow_8bit(SixMatrix_Context_t *mat, uint8_t *colBytes, uint8_t rowNum)
{
	sim_printrow(colBytes, SIXMATRIX_NUM_COL_BYTES_8BIT, rowNum);

	gdk_threads_enter();

	int colNum;

	for (colNum = 0; colNum < SIXMATRIX_NUM_COL_BYTES_8BIT; colNum++) {
		drawLed(
			colNum,
			rowNum,
			(colBytes[colNum] & 0b11110000) >> 4,
			(colBytes[colNum] & 0b00001111)
			);
	}

	gtk_widget_draw(drawing_area, NULL);
	gdk_threads_leave();
}
