#include "idle.h"

#if 0
void idle_thruster_listener_func(IdleThrusterListener *listener);
#endif
void idle_update(IdleAct *idle);
void idle_set_active(IdleAct *idle, r_bool nowactive);
void idle_broadcast(IdleAct *idle);

void init_idle(IdleAct *idle)
{
	idle->func = (ActivationFunc) idle_update;
	idle->num_handlers = 0;
	idle->nowactive = FALSE;
	idle->last_touch = clock_time_us();
	idle_set_active(idle, TRUE); 
#if 0
	idle->thrusterListener.func = (ThrusterUpdateFunc) idle_thruster_listener_func;
	idle->thrusterListener.idle = idle;
#endif
	schedule_us(1, (Activation*) idle);
}

#if 0
void idle_thruster_listener_func(IdleThrusterListener *listener)
{
	idle_touch(listener->idle);
}
#endif

void idle_add_handler(IdleAct *idle, UIEventHandler *handler)
{
	assert(idle->num_handlers < MAX_IDLE_HANDLERS);
	idle->handlers[idle->num_handlers] = handler;
	idle->num_handlers += 1;
	idle_broadcast(idle);
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
		idle_broadcast(idle);
	}
}

void idle_broadcast(IdleAct *idle)
{
	int i;
	UIEvent evt = idle->nowactive ? evt_idle_nowactive : evt_idle_nowidle;

	//LOGF((logfp, "idle_broadcast(%d => %d)", idle->nowactive, evt));

	for (i=0; i<idle->num_handlers; i++)
	{
		idle->handlers[i]->func(idle->handlers[i], evt);
	}
}

void idle_touch(IdleAct *idle)
{
	idle->last_touch = clock_time_us();
	idle_set_active(idle, TRUE);
}

