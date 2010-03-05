#ifndef _hpam_h
#define _hpam_h

#include "rocket.h"
#include "thruster_protocol.h"

typedef struct {
	r_bool status;
	uint8_t rest_time_secs;
	Time expire_time;	// time when needs to rest, or when rest complete
	r_bool resting;
	uint8_t board;
	uint8_t digit;
	uint8_t segment;
} HPAMPort;

typedef enum {
	hpam_hobbs = 0,
	hpam_clanger = 1,
	hpam_hatch_solenoid_reserved = 2,
	hpam_lighting_flicker = 3,	// 5V
	hpam_thruster_frontleft = 4,
	hpam_thruster_frontright = 5,
	hpam_thruster_rear = 6,
	hpam_booster = 7,
	hpam_end
} HPAMIndex;

// Future: gauges on a 12V HPAM.

typedef struct {
	ActivationFunc func;
	HPAMPort hpam_ports[hpam_end];
	ThrusterUpdate **thrusterUpdates;
	BoardBuffer bbuf;
	ThrusterPayload thrusterPayload;
} HPAM;

void init_hpam(HPAM *hpam, uint8_t board0, ThrusterUpdate **thrusterUpdates);
void hpam_set_port(HPAM *hpam, HPAMIndex idx, r_bool status);
r_bool hpam_get_port(HPAM *hpam, HPAMIndex idx);

#endif // _hpam_h
