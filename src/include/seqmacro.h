/************************************************************************
 *
 * This file is part of RulOS, Ravenna Ultra-Low-Altitude Operating
 * System -- the free, open-source operating system for microcontrollers.
 *
 * Written by Jon Howell (jonh@jonh.net) and Jeremy Elson (jelson@gmail.com),
 * May 2009.
 *
 * This operating system is in the public domain.  Copyright is waived.
 * All source code is completely free to use by anyone for any purpose
 * with no restrictions whatsoever.
 *
 * For more information about the project, see: www.jonh.net/rulos
 *
 ************************************************************************/

#ifndef _SEQMACRO_H
#define _SEQMACRO_H

#define SEQDECL(T,f,i)	\
void T##_##f##_##i(Activation *act)

#define SEQDEF(T,f,i,t)	\
SEQDECL(T,f,i) \
{ \
	T *t = ((T*) act); \
	SYNCDEBUG();

#define SEQNEXT(T,f,i) \
	act->func = &T##_##f##_##i; \
	//syncdebug(3, 'a', (int) (act->func)<<1)

#endif // _SEQMACRO_H
