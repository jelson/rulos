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

#pragma once

typedef uint8_t Port;

// Wire format for packets.  Format:
// Dest port
// Checksum
// Payload_Len
// [Payload_Len * uint8_t]
typedef struct s_message {
  Port dest_port;
  uint8_t checksum;
  uint8_t payload_len;
  unsigned char data[0];
} Message, *MessagePtr;
