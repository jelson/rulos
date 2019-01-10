#include <math.h>

#include "navigation.h"

#ifdef SIM
#include <stdio.h>

#define NDBG 0
#endif

#define ALPHA (3.0)
// alpha: distance in meters at which we aim 45-degrees towards
// route segment. (closer than this, we aim more shallowly along the
// segment; farther, and we turn more aggressively back to the line.)
float alpha = 1.0 / ALPHA;

#define MIN_PER_DEGREE (60.0)
#define NM_PER_MIN_Y (1.0)
#define METERS_PER_NM (1852.0)
#define RAD_TO_DEG(rad) ((rad)*180.0 / PI)
#define DEG_TO_RAD(rad) ((rad)*PI / 180.0)

float s_dot(Vector *v0, Vector *v1) { return v0->x * v1->x + v0->y * v1->y; }

float s_len(Vector *v) { return sqrt(s_dot(v, v)); }

void v_rot_quarter_ccw(Vector *out, Vector *in) {
  out->x = -in->y;
  out->y = in->x;
}

void v_rot_quarter_cw(Vector *out, Vector *in) {
  out->x = in->y;
  out->y = -in->x;
}

void v_copy(Vector *out, Vector *in) {
  out->x = in->x;
  out->y = in->y;
}

#define PI (3.141592653589)
#define SQRT2OVER2 (0.707106781187)
#define EPSILON (0.01)
// angle conversion quandrant doesn't have to be precise

float s_unit_to_angle(Vector *u_in)
// Given a unit vector, return an angle in (-pi,pi].
{
  Vector u, tmp;
  v_copy(&u, u_in);
  float compensation = 0.0;
  while (u.x <= SQRT2OVER2 - EPSILON) {
    if (u.y >= 0) {
      compensation += PI / 2.0;
      v_copy(&tmp, &u);
      v_rot_quarter_cw(&u, &tmp);
    } else {
      compensation -= PI / 2.0;
      v_copy(&tmp, &u);
      v_rot_quarter_ccw(&u, &tmp);
    }
  }
  // Now u.x is both positive and not close to zero,
  // so the quotient is well-conditioned.
  float result = atan(u.y / u.x);
  result += compensation;
  return result;
}

void v_init(Vector *out, float x, float y) {
  out->x = x;
  out->y = y;
}

void v_add(Vector *out, Vector *v1, Vector *v2) {
  out->x = v1->x + v2->x;
  out->y = v1->y + v2->y;
}

void v_sub(Vector *out, Vector *v1, Vector *v2) {
  out->x = v1->x - v2->x;
  out->y = v1->y - v2->y;
}

void v_transform(Vector *out, Vector *v, Vector *o, Vector *u)
// transform vector v into the coordinate system defined by
// origin o and x-axis unit-vector u
{
  Vector ov;
  v_sub(&ov, v, o);
  out->x = s_dot(&ov, u);
  Vector u_y;
  v_rot_quarter_ccw(&u_y, u);
  out->y = s_dot(&ov, &u_y);
}

void v_normalize(Vector *inout) {
  float factor = 1.0 / s_len(inout);
  inout->x *= factor;
  inout->y *= factor;
}

int navigation_compute(Navigation *nav, Vector *p0_ll, Vector *p1_ll) {
  Vector p0;
  nav_meters_from_ll(nav, &p0, p0_ll);
  Vector p1;
  nav_meters_from_ll(nav, &p1, p1_ll);

#if NDBG
  fprintf(stderr, "p0(%f,%f)\n", (double)p0.x, (double)p0.y);
  fprintf(stderr, "p1(%f,%f)\n", (double)p1.x, (double)p1.y);
#endif

  // waypoint space has w_0 at its origin, w_1 on the x axis.
  Vector p1_w;  // position in waypoint-space
  v_transform(&p1_w, &p1, &nav->w0, &nav->waypoint_unit);
#if NDBG
  fprintf(stderr, "p1_w(%f,%f)\n", (double)p1_w.x, (double)p1_w.y);
  fprintf(stderr, "waypoint_dist %f\n", (double)nav->waypoint_dist);
#endif

  if (p1_w.x > nav->waypoint_dist) {
    // we're abeam! Tell caller to sequence
    return ABEAM;
  }

  Vector desired_w;  // desired heading: a unit-vector in waypoint_space
  Vector recover;
  recover.x = 1.0;
  recover.y = -alpha * p1_w.y;
  v_copy(&desired_w, &recover);
  // v_add(&desired_w, &nav->waypoint_unit, &recover);
  v_normalize(&desired_w);
#if NDBG
  fprintf(stderr, "recover(%f,%f)\n", (double)recover.x, (double)recover.y);
  fprintf(stderr, "desired_w(%f,%f) l=%f\n", (double)desired_w.x,
          (double)desired_w.y, (double)s_len(&desired_w));
#endif

  Vector p0_w;  // old position in waypoint-space
  v_transform(&p0_w, &p0, &nav->w0, &nav->waypoint_unit);
#if NDBG
  fprintf(stderr, "p0_w(%f,%f)\n", (double)p0_w.x, (double)p0_w.y);
#endif
  Vector heading_w;  // current heading: a unit-vector in waypoint-space.
  v_sub(&heading_w, &p1_w, &p0_w);
  v_normalize(&heading_w);
#if NDBG
  fprintf(stderr, "heading_w(%f,%f) l=%f\n", (double)heading_w.x,
          (double)heading_w.y, (double)s_len(&heading_w));
#endif

  Vector heading_d;  // current heading in desired-heading-space;
                     // (1,0)=>ideal heading
  Vector origin;
  v_init(&origin, 0, 0);
  v_transform(&heading_d, &heading_w, &origin, &desired_w);
#if NDBG
  fprintf(stderr, "heading_d(%f,%f) l=%f\n", (double)heading_d.x,
          (double)heading_d.y, (double)s_len(&heading_d));
#endif

  float angle_radians = s_unit_to_angle(&heading_d);
#if NDBG
  fprintf(stderr, "angle_radians = %f\n", angle_radians);
#endif

  // angle_radians is positive-ccw-radians; we want to correct (one negation)
  // and also invert to positive-cw-degrees (another negation).
  int command_angle_degrees = (int)RAD_TO_DEG(angle_radians);
  return command_angle_degrees;
}

void navigation_init(Navigation *nav, Vector *system_origin) {
  v_copy(&nav->system_origin, system_origin);
}

void navigation_activate_leg(Navigation *nav, Vector *w0_ll, Vector *w1_ll) {
  Vector w0;
  nav_meters_from_ll(nav, &w0, w0_ll);
  Vector w1;
  nav_meters_from_ll(nav, &w1, w1_ll);

  v_sub(&nav->waypoint_unit, &w1, &w0);
  v_copy(&nav->w0, &w0);
  nav->waypoint_dist = s_len(&nav->waypoint_unit);
  v_normalize(&nav->waypoint_unit);
}

void nav_meters_from_ll(Navigation *nav, Vector *out, Vector *in) {
  Vector deg;
  v_sub(&deg, in, &nav->system_origin);
  // deg is now "small", since 'in' is 'near' the system_origin.
  // so errors in projecting to a flat map are small. (too rushed to do
  // spherical math)
  float nm_per_min_x = cos(DEG_TO_RAD(nav->system_origin.y));
  out->x = deg.x * MIN_PER_DEGREE * nm_per_min_x * METERS_PER_NM;
  out->y = deg.y * MIN_PER_DEGREE * NM_PER_MIN_Y * METERS_PER_NM;
}
