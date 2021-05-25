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

// Addresses
#define ROCKET_ADDR  (0x01)
#define ROCKET1_ADDR (0x02)
#define AUDIO_ADDR   (0x03)

#define DONGLE_BASE_ADDR (0x50) /* Address of 0th dongle */
/*
#define DONGLE1_ADDR                    (0x51)
#define DONGLE2_ADDR                    (0x52)
#define DONGLE3_ADDR                    (0x53)
#define DONGLE4_ADDR                    (0x54)
#define DONGLE5_ADDR                    (0x55)
#define DONGLE6_ADDR                    (0x56)
#define DONGLE7_ADDR                    (0x57)
*/

// Ports
#define THRUSTER_PORT         (0x11)
#define REMOTE_BBUF_PORT      (0x12)
#define REMOTE_KEYBOARD_PORT  (0x13)
#define REMOTE_SUBFOCUS_PORT0 (0x14) /* remote_keyboard protocol */
#define AUDIO_PORT            (0x15)
#define SCREENBLANKER_PORT    (0x16)
#define UARTNETWORKTEST_PORT  (0x17)
#define STREAMING_AUDIO_PORT  (0x18)
#define SET_VOLUME_PORT       (0x19)
#define MUSIC_CONTROL_PORT    (0x20)
#define MUSIC_METADATA_PORT   (0x21)
