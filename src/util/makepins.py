#!/usr/bin/python

for port in ['B', 'C', 'D']:
    for pin in ['0', '1', '2', '3', '4', '5', '6', '7']:
        print "#define GPIO_%s%s  (&DDR%s), (&PORT%s), (&PIN%s), (PORT%s%s)" %\
        (port, pin, port, port, port, port, pin)
        
