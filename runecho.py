#!/usr/bin/env python

import sys
import serial
import time
from struct import Struct
import array

doswap=(sys.byteorder=='little')

if len(sys.argv)<2:
	port='/dev/ttyACM0'
else:
	port=sys.argv[1]


def crc16(data):
	S=0xffff

	for d in data:
		S ^= ord(d)
		for n in range(8):
			b=S&1
			S>>=1
			if b:
				S ^= 0xa001;
	return S

def addcrc(data):
	crc=crc16(data)
	H=crc>>8
	L=crc&0xff;
	return '%s%s%s'%(data,chr(L),chr(H))

td=b'\x01\x03\x12\x34\x00\x04'
assert crc16(td)==0xbf00
del td

_H=Struct('<H')
_M=Struct('!BBHH')

def setreg(S, addr, val):
	print('Set '+hex(addr)+' to '+hex(val))
	M = addcrc(_M.pack(1,6,addr,val))
	print('<<',M)
	S.write(M)
	R = S.read(len(M))
	print('>>',R)
	print('ok' if R==M else 'mis-match')

def getregs(S, addr, count=1):
	assert(count>=1 and count<128)
	M = addcrc(_M.pack(1,3,addr,count))
	print('<<',M)
	S.write(M)
	header = S.read(3)
	print('>>',header)
	if(ord(header[1])!=3):
		raise RuntimeError('Invalid response')
	body = S.read(2*(1+ord(header[2])))
	print('>>',body)
	crcr = crc16(header+body[:-2])
	crce, = _H.unpack(body[-2:])
	#print('crc',crcr,crce)
	if crcr!=crce:
	      print('CRC mismatch',hex(crcr),hex(crce))
	      raise RuntimeError('CRC')
	D = array.ArrayType('H')
	D.fromstring(body[:-2])
	if doswap:
	      D.byteswap()
	return list(D)

def main():
	print('Opening',port)
	S = serial.Serial(port, 115200, timeout=1)

	print("Wait for reboot")
	time.sleep(2.0)

	E=[0x1234,0x5678,0x1020,0x3040]

	setreg(S,0,E[0])
	setreg(S,1,E[1])
	setreg(S,2,E[2])
	setreg(S,3,E[3])
	
	D = getregs(S,0,4)
	if D==E:
		print('Data matches')
	else:
		print('expect',E)
		print("recv'd",D)

	S.close()

if __name__=='__main__':
	main()
