#pragma once

typedef struct {
  float x, y;
} Vector;

void v_init(Vector *out, float x, float y);
void v_copy(Vector *out, Vector *in);

typedef struct {
  Vector system_origin;  // defines latitude, for calibrating map in meters
  float waypoint_dist;
  Vector w0;
  Vector waypoint_unit;
} Navigation;

void navigation_init(Navigation *nav, Vector *system_origin);

void navigation_activate_leg(Navigation *nav, Vector *w0_ll, Vector *w1_ll);

#define ABEAM (300)
int navigation_compute(Navigation *nav, Vector *p0_ll, Vector *p1_ll);
// p0 is a previous position from which we compute our heading;
// p1 is our most-recently-known position.
// returns the angle we should turn, in degrees, or ABEAM if we've
// passed this waypoint.

void nav_meters_from_ll(Navigation *nav, Vector *out, Vector *in);
