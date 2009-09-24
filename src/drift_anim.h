#ifndef drift_anim_h
#define drift_anim_h

#include <inttypes.h>

typedef struct {
	// bounds
	uint8_t expscale;	// all internal values scaled by 1<<expscale
	int32_t min;
	int32_t max;
	uint32_t maxSpeed;	// per 1024ms, absolute value of velocity

	// state
	int32_t base;
	uint32_t base_time;
	int32_t velocity;	// per 1024ms
	uint32_t last_impulse_time;	// clock_time
} DriftAnim;

void drift_anim_init(DriftAnim *da, uint8_t expscale, int32_t initValue, int32_t min, int32_t max, uint32_t maxSpeed);

int32_t da_read(DriftAnim *da);
void da_random_impulse(DriftAnim *da);
void da_set_velocity(DriftAnim *da, int32_t velocity);
void da_set_value(DriftAnim *da, int32_t value);

#endif // drift_anim_h
