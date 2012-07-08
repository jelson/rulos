#pragma once

typedef struct {
	float x, y;
} Vector;

void v_init(Vector *out, float x, float y);

typedef struct {
	float waypoint_dist;
	Vector w0;
	Vector waypoint_unit;
} Navigation;

void navigation_activate_leg(Navigation *nav, Vector *w0, Vector *w1);

#define ABEAM	(300)
int navigation_compute(Navigation *nav, Vector *p0, Vector *p1);
	// p0 is a previous position from which we compute our heading;
	// p1 is our most-recently-known position.
	// returns the angle we should turn, in degrees, or ABEAM if we've
	// passed this waypoint.
