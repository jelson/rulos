#include "control.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "core/clock.h"
#include "core/logging.h"
#include "leds.h"

#define OBSERVATION_DURATION_SAMPLES 5
#define US_PER_DEGREE ((uint32_t)(5000000 / 360))
// That's one minute for a 360-degree turn.
#define MOTOR_POWER 75

typedef enum {
  RUDDER_RIGHT = 0,
  RUDDER_LEFT = 1,
  RUDDER_STRAIGHT = 2
} RudderRequest;

void _control_start_observing(void *v_ctl);
void _control_gps_read(void *v_ctl);
void _control_observe(Control *ctl);
void _control_start_turning(Control *ctl);
void _control_test_rudder_act(void *v_ctl);

void _control_set_rudder(Control *ctl, RudderRequest req) {
  switch (req) {
    case RUDDER_STRAIGHT:
      rudder_set_angle(&ctl->rudder, -15);
      break;
    case RUDDER_RIGHT:
      rudder_set_angle(&ctl->rudder, -50);
      break;
    case RUDDER_LEFT:
      rudder_set_angle(&ctl->rudder, 15);
      break;
  }
}

void control_set_motor_state(Control *ctl, bool onoff) {
  motors_set_power(&ctl->motors, onoff ? MOTOR_POWER : 0);
  leds_green(onoff);
}

void control_init(Control *ctl) {
  ctl->state = Acquiring;
  rudder_init(&ctl->rudder);
  motors_init(&ctl->motors);
  control_set_motor_state(ctl, 0);

  ctl->n_waypoints = 0;
}

void control_test_rudder(Control *ctl) {
  ctl->test_rudder_state = 0;
  ctl->test_rudder_count = 0;
  motors_set_power(&ctl->motors, MOTOR_POWER);
  leds_green(1);
  schedule_us(1000, _control_test_rudder_act, ctl);
}

void _control_test_rudder_act(void *v_ctl) {
  Control *ctl = (Control *)v_ctl;
  ctl->test_rudder_count += 1;
  if (ctl->test_rudder_count > 9) {
    motors_set_power(&ctl->motors, 0);
    return;
  }
  ctl->test_rudder_state = (ctl->test_rudder_state + 1) % 3;
  _control_set_rudder(ctl, (RudderRequest)ctl->test_rudder_state);
  schedule_us(5000000, _control_test_rudder_act, ctl);
}

void control_add_waypoint(Control *ctl, Vector *wpt) {
  assert(ctl->n_waypoints < MAX_WAYPOINTS);
  v_copy(&ctl->waypoints[ctl->n_waypoints], wpt);
  ctl->n_waypoints += 1;
}

void _control_sequence_waypoint(Control *ctl, int index) {
  assert(index > 0);
  assert(index < ctl->n_waypoints);
  ctl->waypoint_index = index;
  navigation_activate_leg(&ctl->nav, &ctl->waypoints[ctl->waypoint_index - 1],
                          &ctl->waypoints[ctl->waypoint_index]);
}

void control_start(Control *ctl) {
  navigation_init(&ctl->nav, &ctl->waypoints[0]);

  _control_sequence_waypoint(ctl, 1);
  gpsinput_init(&ctl->gpsi, 1, _control_gps_read, ctl);
}

void _test_sentence_done(GPSInput *gpsi) {
  LOG("Read %f,%f", (double)gpsi->lat, (double)gpsi->lon);
}

void _control_gps_read(void *v_ctl) {
  Control *ctl = (Control *)v_ctl;

  _test_sentence_done(&ctl->gpsi);

  switch (ctl->state) {
    case Acquiring:
      schedule_now(_control_start_observing, ctl);
      return;

    case Observing:
      _control_observe(ctl);
      return;

    case Turning:
      // ignore GPS while turning
      return;

    case Completed:
      // ignore GPS when we're done!
      return;
  }
}

void _control_start_observing(void *v_ctl) {
  LOG("_control_start_observing");
  Control *ctl = (Control *)v_ctl;
  _control_set_rudder(ctl, RUDDER_STRAIGHT);
  ctl->sample_num = 0;
  ctl->state = Observing;
  control_set_motor_state(ctl, 1);
  // next time GPS arrives, we'll accumulate data.
}

void _control_observe(Control *ctl) {
  ctl->sample_num += 1;
  // We discard the first sample, as it reflects observations
  // from the prior turning period.
  if (ctl->sample_num == 2) {
    v_init(&ctl->p0, ctl->gpsi.lon, ctl->gpsi.lat);
  } else if (ctl->sample_num > OBSERVATION_DURATION_SAMPLES) {
    v_init(&ctl->p1, ctl->gpsi.lon, ctl->gpsi.lat);
    _control_start_turning(ctl);
  }
}

#define INTABS(x) (((x) > 0) ? (x) : (-x))

void _control_start_turning(Control *ctl) {
  int turn_request;
  while (TRUE) {
    turn_request = navigation_compute(&ctl->nav, &ctl->p0, &ctl->p1);
    if (turn_request == ABEAM) {
      LOG("sequence waypoint");
      int next_index = ctl->waypoint_index + 1;
      if (next_index >= ctl->n_waypoints) {
        // OMFSM DONE!
        LOG("task complete");
        ctl->state = Completed;
        control_set_motor_state(ctl, 0);
        return;
      }
      _control_sequence_waypoint(ctl, next_index);
      continue;
    }
    // have a valid turn request
    break;
  }

  LOG("turn %s %d deg", turn_request > 0 ? "right" : "left", turn_request);
  _control_set_rudder(ctl, turn_request > 0 ? RUDDER_RIGHT : RUDDER_LEFT);
  Time turn_duration_us = INTABS(turn_request) * US_PER_DEGREE;
  ctl->state = Turning;
  schedule_us(turn_duration_us, _control_start_observing, ctl);
}
