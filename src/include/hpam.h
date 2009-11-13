#ifndef _hpam_h
#define _hpam_h

#include "rocket.h"
#include "thruster_protocol.h"

typedef struct {
	r_bool status;
	Time expire_time;
	Time max_time;
	r_bool resting;
	Time rest_until;
	uint8_t board;
	uint8_t digit;
	uint8_t segment;
} HPAMPort;

typedef enum {
	hpam_empty_0 = 0,
	hpam_empty_1 = 1,
	hpam_empty_2 = 2,
	hpam_empty_3 = 3,
	hpam_thruster_frontleft = 4,
	hpam_thruster_frontright = 5,
	hpam_thruster_rear = 6,
	hpam_booster = 7,
	hpam_end
} HPAMIndex;

typedef struct {
	ActivationFunc func;
	HPAMPort hpam_ports[hpam_end];
	ThrusterUpdate **thrusterUpdates;
	BoardBuffer bbuf;
} HPAM;

void init_hpam(HPAM *hpam, uint8_t board0, ThrusterUpdate **thrusterUpdates);
void hpam_set_port(HPAM *hpam, HPAMIndex idx, r_bool status);

#endif // _hpam_h
