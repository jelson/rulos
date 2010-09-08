#!/usr/bin/python

#
# This file is part of RulOS, Ravenna Ultra-Low-Altitude Operating
# System -- the free, open-source operating system for microcontrollers.
#
# Written by Jon Howell (jonh@jonh.net) and Jeremy Elson (jelson@gmail.com),
# May 2009.
#
# This operating system is in the public domain.  Copyright is waived.
# All source code is completely free to use by anyone for any purpose
# with no restrictions whatsoever.
#
# For more information about the project, see: www.jonh.net/rulos
#

for port in ['B', 'C', 'D']:
    for pin in ['0', '1', '2', '3', '4', '5', '6', '7']:
        print "#define GPIO_%s%s  (&DDR%s), (&PORT%s), (&PIN%s), (PORT%s%s)" %\
        (port, pin, port, port, port, port, pin)
        
