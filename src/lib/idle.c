#include "idle.h"

void idle_thruster_listener_func(IdleThrusterListener *listener);
void idle_update(IdleAct *idle);
void idle_set_active(IdleAct *idle, r_bool nowactive);

void init_idle(IdleAct *idle)
{
	idle->func = (ActivationFunc) idle_update;
	idle->num_handlers = 0;
	idle->nowactive = TRUE;
		// first event guaranteed to be a nowidle event
	idle->last_touch = clock_time_us();
	idle->thrusterListener.func = (ThrusterUpdateFunc) idle_thruster_listener_func;
	idle->thrusterListener.idle = idle;
	schedule_us(1, (Activation*) idle);
}

void idle_thruster_listener_func(IdleThrusterListener *listener)
{
	idle_touch(listener->idle);
}

void idle_add_handler(IdleAct *idle, UIEventHandler *handler)
{
	assert(idle->num_handlers < MAX_IDLE_HANDLERS);
	idle->handlers[idle->num_handlers] = handler;
	idle->num_handlers += 1;
}

void idle_update(IdleAct *idle)
{
	schedule_us(1000000, (Activation*) idle);
	if (later_than(clock_time_us() - IDLE_PERIOD, idle->last_touch))
	{
		// system is idle.
		idle_set_active(idle, FALSE);
		// prevent wrap around by walking an old last-touch time forward
		idle->last_touch = clock_time_us() - IDLE_PERIOD - 1;
	}
}

void idle_set_active(IdleAct *idle, r_bool nowactive)
{
	if (nowactive != idle->nowactive)
	{
		idle->nowactive = nowactive;
		int i;
		for (i=0; i<idle->num_handlers; i++)
		{
			idle->handlers[i]->func(
				idle->handlers[i],
				nowactive ? evt_idle_nowactive : evt_idle_nowidle);
		}
	}
}

void idle_touch(IdleAct *idle)
{
	idle->last_touch = clock_time_us();
	idle_set_active(idle, TRUE);
}

