#!/usr/bin/env python
"""Plot results of digitizing

Microchip MCP 9701A Linear Active Thermistor
"""

import sys

from matplotlib import pylab as pl
import numpy as np
from scipy.constants import C2F
from scipy.optimize import leastsq

raw=np.loadtxt(sys.argv[1])

print raw.shape

#raw[:,1] /= 1023.0 # Normalize to ADC range (10-bit)
#raw[:,1] *= 5.0 # Convert to volts
## Convert voltage to temperature
## Expect (20 mV/C) 400 mV = 0 C
#raw[:,1] -= 0.4
#raw[:,1] /= 0.02
#raw[:,1] = C2F(raw[:,1])

raw[:,0] -= raw[0,0] # time relative to start
raw[:,0] /= 60.0 # min.

T, V = raw[:,0], raw[:,2]

print 'Fit rise'

tstart=0.2

t0=pl.find(T>tstart)[0]
t1=V.argmax()
print 'fitting between',T[t0],'and',T[t1]

def fn1eval(x, p):
    # [ amplitude, 0 offset, time const ]
    return p[0]*(1-np.exp(-x/p[2])) + p[1]

def fn1err(p, x, y):
    return y - fn1eval(x,p)

p0 = [ V[t1]-V[t0], V[t0], 1]
print 'initial',p0

pr,_ = leastsq(fn1err, p0, args=(T[t0:t1], V[t0:t1]))
print 'opt',pr

#Vi = fn1eval(T[t0:t1], p0)
Tr=T[t0:]
Vr = fn1eval(Tr, pr)

print 'Fit fall'

t0=t1 # start from peek
t1=len(V)-1 # to end
print 'fitting between',T[t0],'and',T[t1]

def fn2eval(x, p):
    # [ amplitude, 0 offset, time const ]
    return p[0]*np.exp(-x/p[2]) + p[1]

def fn2err(p, x, y):
    return y - fn2eval(x,p)

p0 = pr #[ V[t0]-V[t1], V[t1], pr[2]]
print 'initial',p0

# Shift high point to t=0 so that parameters are comparable
pf,_ = leastsq(fn2err, p0, args=(T[t0:t1]-T[t0], V[t0:t1]))
print 'opt',pf

Tf=T[t0:t1]
Vf = fn2eval(Tf-T[t0], pf)

print 'time constants'
print 'rise %.2f m'%pr[2]
print 'fall %.2f m'%pf[2]
print 'max temp %.1f F'%(pr[0]+pr[1])

pl.plot(T, V, 'b-', Tr, Vr, 'r-', Tf, Vf, 'r-')
pl.xlabel('time (m)')
pl.ylabel('temp. (F)')
pl.show()
