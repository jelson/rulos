
typedef struct s_MotorState
{
	uint8_t desired_power;
	uint8_t test_mode;
} MotorState;

void motors_init(MotorState *motors);
void motors_set_power(MotorState *motors, uint8_t power);
void motors_test_mode(MotorState *motors);
