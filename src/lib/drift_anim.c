#include "rocket.h"
#include "drift_anim.h"

void drift_anim_init(DriftAnim *da, uint8_t expscale, int32_t initValue, int32_t min, int32_t max, uint32_t maxSpeed)
{
	da->expscale = expscale;
	da->min = min << expscale;
	da->max = max << expscale;
	da->maxSpeed = maxSpeed << expscale;
	da_random_impulse(da);	// assign a valid velocity.
	da->base = initValue;
	da->base_time = clock_time_us();
}

int32_t _da_eval(DriftAnim *da, Time t)
{
	int32_t dt = (t - da->base_time)/1000;
	int32_t nv = da->base + ((da->velocity * dt) >> 10);
	int32_t res = bound(nv, da->min, da->max);
	return res;
}

int32_t da_read(DriftAnim *da)
{
	return _da_eval(da, clock_time_us()) >> da->expscale;
}

void _da_update_base(DriftAnim *da)
{
	Time t = clock_time_us();
	da->base = _da_eval(da, t);
	da->base_time = t;
}

void da_random_impulse(DriftAnim *da)
{
	_da_update_base(da);
	// select a new velocity
	da->velocity = (deadbeef_rand() % (da->maxSpeed*2+1)) - da->maxSpeed;
}

void da_set_velocity(DriftAnim *da, int32_t velocity)
{
	_da_update_base(da);
	da->velocity = bound(velocity << da->expscale, -da->maxSpeed, da->maxSpeed);
}

void da_set_value(DriftAnim *da, int32_t value)
{
	da->base = value << da->expscale;
	da->base_time = clock_time_us();
}
