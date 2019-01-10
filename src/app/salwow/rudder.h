#include <stdint.h>

typedef struct s_RudderState {
  int8_t desired_position;

  // test mode
  int8_t test_mode;
  int8_t delay_timer;
  int8_t next_mode;

} RudderState;

void rudder_init(RudderState *rudder);
void rudder_set_angle(RudderState *rudder, int8_t position);
void rudder_test_mode(RudderState *rudder);
