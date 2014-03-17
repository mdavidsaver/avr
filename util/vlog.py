#!/usr/bin/env python
"""Log results of digitizing

Microchip MCP 9701A Linear Active Thermistor
"""

import sys, time

from scipy.constants import C2F

F, O = open(sys.argv[1], 'r'), None
if len(sys.argv)>2:
    O = open(sys.argv[2], 'w')

while True:
    rval= int(F.readline().rstrip(), 16)
    # Scale to voltage 0x7ff=5V
    # 0C offset is 0.4V
    # Scale is 0.02 V/C
    # Covert to F
    val = C2F(((5.0/1023)*rval - 0.4)/0.02)
    ts = time.time()
    print ts, rval, '%.02f'%val
    if O is not None:
        print >>O,ts, rval, val
        O.flush()
