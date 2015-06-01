#!/usr/bin/env python

import sys
from ctypes import create_string_buffer
import struct
import usb

val = 42
if len(sys.argv)>1:
    if sys.argv[1]=='reset':
        val = sys.argv[1]
    else:
        val = int(sys.argv[1], 0)

def finddev():
    buslist = usb.busses()
    for bus in buslist:
        print 'visit bus', bus.location
        for dev in bus.devices:
            print 'visit device',hex(dev.idVendor),hex(dev.idProduct)
            if dev.idVendor==0x1234 and dev.idProduct==0x1234:
                return dev
    raise RuntimeError("No device found")

def controlRead(H, RT, R, N, fmt=None):
    assert RT&0x80!=0, bin(RT)
    T = H.controlMsg(RT, R, N)
    R = ''.join(map(chr,T))
    if fmt:
        R = struct.unpack(fmt, R)
    return R

def controlWrite(H, RT, R, buf):
    assert RT&0x80==0, bin(RT)
    buf = tuple(map(ord, buf))
    return H.controlMsg(RT, R, buf)

D=finddev()

H=D.open()
iface = D.configurations[0].interfaces[0][0]
#H.detachKernelDriver()
H.claimInterface(iface)

if val=='reset':
    print val
    H.reset()
    sys.exit(0)

# Read initial value
print 'R', hex(controlRead(H, 0b11000010, 0x7f, 2, fmt='<H')[0])

# Update value
print 'W', controlWrite(H, 0b01000010, 0x7f, struct.pack('<H', val))

# Read new value
print 'R', hex(controlRead(H, 0b11000010, 0x7f, 2, fmt='<H')[0])
