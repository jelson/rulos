#include <stdio.h>

#include <gtk/gtk.h>

#define X_LEDS 24
#define Y_LEDS 16
#define LED_RADIUS 20
#define LED_SPACING 8

GtkWidget *topWindow = NULL;
GtkWidget *drawing_area = NULL;
GdkPixmap *canvas = NULL;
GdkGC *canvas_gc = NULL;

void destroy (void)
{
  gtk_main_quit();
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

static gint drawLed(int x, int y, int rColor, int gColor)
{
  GdkColor ledColor = { 0, rColor * (0xffff/16), gColor * (0xffff/16), 0 };
  GdkColor ledOutline = { 0, 0x4000, 0x4000, 0x4000 };

  gdk_gc_set_rgb_fg_color(canvas_gc, &ledColor);

  gdk_draw_arc(canvas,
	       canvas_gc,
	       TRUE,
	       LED_SPACING/2 + x*(LED_SPACING+LED_RADIUS),
	       LED_SPACING/2 + y*(LED_SPACING+LED_RADIUS),
	       LED_RADIUS, LED_RADIUS,
	       0, 360*64);

  gdk_gc_set_rgb_fg_color(canvas_gc, &ledOutline);

  gdk_draw_arc(canvas,
	       canvas_gc,
	       FALSE,
	       LED_SPACING/2 + x*(LED_SPACING+LED_RADIUS),
	       LED_SPACING/2 + y*(LED_SPACING+LED_RADIUS),
	       LED_RADIUS, LED_RADIUS,
	       0, 360*64);


}

static gint initCanvas()
{
  canvas = gdk_pixmap_new(topWindow->window,
			  topWindow->allocation.width, 
			  topWindow->allocation.height,
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


int main (int argc, char *argv[])
{
	gtk_init(&argc, &argv);

	topWindow = gtk_window_new(GTK_WINDOW_TOPLEVEL);

	gtk_window_set_default_size(GTK_WINDOW(topWindow),
								X_LEDS*(LED_RADIUS + LED_SPACING),
								Y_LEDS*(LED_RADIUS + LED_SPACING));
	
	gtk_signal_connect(GTK_OBJECT (topWindow), "destroy",
						GTK_SIGNAL_FUNC (destroy), NULL);
	
	drawing_area = gtk_drawing_area_new();
	gtk_signal_connect(GTK_OBJECT (drawing_area), "expose_event",
						GTK_SIGNAL_FUNC (expose_topWindow), NULL);
	
	gtk_widget_show(topWindow);
	
	gtk_container_add(GTK_CONTAINER (topWindow), drawing_area);
	gtk_widget_show (drawing_area);
	
	initCanvas();
	
	gtk_main();

	return 0;
}
