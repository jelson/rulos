/*
 * Copyright (C) 2009 Jon Howell (jonh@jonh.net) and Jeremy Elson
 * (jelson@gmail.com).
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */

#pragma once

#if defined(BOARDCONFIG_ROCKET0)
#define NUM_LOCAL_BOARDS 8
#define NUM_REMOTE_BOARDS 0
#elif defined(BOARDCONFIG_NETROCKET)  // A bench-only configuration
#define NUM_LOCAL_BOARDS 0
#define NUM_REMOTE_BOARDS 8
#elif defined(BOARDCONFIG_UNIROCKET)
#define NUM_LOCAL_BOARDS 0
#define NUM_REMOTE_BOARDS 14
#elif defined(BOARDCONFIG_ROCKETSOUTHBRIDGE)
#define NUM_LOCAL_BOARDS 0
#define NUM_REMOTE_BOARDS 0
#elif defined(BOARDCONFIG_UNIROCKET_LOCALSIM)
#define NUM_LOCAL_BOARDS 14
#define NUM_REMOTE_BOARDS 0
#elif defined(BOARDCONFIG_ROCKET1)
#define NUM_LOCAL_BOARDS 4
#define NUM_REMOTE_BOARDS 0
#elif defined(BOARDCONFIG_WALLCLOCK)
#define NUM_LOCAL_BOARDS 1
#define NUM_REMOTE_BOARDS 0
#elif defined(BOARDCONFIG_CHASECLOCK)
#define NUM_LOCAL_BOARDS 1
#define NUM_REMOTE_BOARDS 0
#elif defined(BOARDCONFIG_DEFAULT)
#define NUM_LOCAL_BOARDS 8
#define NUM_REMOTE_BOARDS 0
#else
#error "Your app Makefile must define one of the BOARDCONFIG_xxx constants."
#include <stop>
#endif
