#define B_MISSION_CLOCK		DBOARD("Mission Clock",			PG PG PG PG PG PG PY PY,  3,  0 )
#define B_LUNAR_DISTANCE	DBOARD("Distance to Moon",		PR PR PR PR PR PR PR PR, 33,  4 )
#define B_SPEED				DBOARD("Speed (fps)",			PY PY PY PY PY PG PG PG, 33,  8 )
#define B_RASTER0			DBOARD("",						PR PR PR PR PR PR PR PR,  9, 12 )
#define B_RASTER1			DBOARD("",						PR PR PR PR PR PR PR PR,  9, 15 )
#define B_RASTER2			DBOARD("",						PR PR PR PR PR PR PR PR,  9, 18 )
#define B_RASTER3			DBOARD("",						PR PR PR PR PR PR PR PR,  9, 21 )
#define B_THRUSTER_ACT		DBOARD("Thruster Actuation",	PB PR PR PR PB PY PY PY,  3, 25 )
#define B_AZE				DBOARD("Azimuth, Elevation",	PG PG PG PR PR PY PY PY, 33, 0  )
#define B_LIQ_HYD_PRES		DBOARD("Liquid Hydrogen Prs",	PG PG PG PG PG PG PG PG,  3, 4  )
#define B_FLIGHTCOMPUTER0	DBOARD("Flight Computer",		PR PR PR PR PY PY PY PY, 17, 9  )
#define B_FLIGHTCOMPUTER1	DBOARD("",						PG PG PG PG PB PB PB PB, 17, 12 )
	
#define T_ROCKET0		\
	B_MISSION_CLOCK,	\
	B_LUNAR_DISTANCE,	\
	B_SPEED,			\
	B_RASTER0,			\
	B_RASTER1,			\
	B_RASTER2,			\
	B_RASTER3,			\
	B_THRUSTER_ACT,		\
	B_END

#define T_ROCKET1		\
	B_AZE,				\
	B_LIQ_HYD_PRES,		\
	B_FLIGHTCOMPUTER0,	\
	B_FLIGHTCOMPUTER1,	\
	B_NO_BOARD			\
	B_NO_BOARD			\
	B_NO_BOARD			\
	B_NO_BOARD			\
	B_END
