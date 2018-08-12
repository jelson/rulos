#pragma once

#include "gpsinput.h"
#include "motors.h"
#include "navigation.h"
#include "rudder.h"

typedef enum { Acquiring, Observing, Turning, Completed } ControlState;

#define MAX_WAYPOINTS 10

typedef struct {
	ControlState state;
	RudderState rudder;
	MotorState motors;
	GPSInput gpsi;
	Navigation nav;

	Vector waypoints[MAX_WAYPOINTS];
	int n_waypoints;
	int waypoint_index;

	Vector p0, p1;

	int sample_num;

	uint8_t test_rudder_state;
	uint8_t test_rudder_count;
} Control;

void control_init(Control *ctl);
void control_add_waypoint(Control *ctl, Vector *wpt);
void control_start(Control *ctl);

void control_test_rudder(Control *ctl);
