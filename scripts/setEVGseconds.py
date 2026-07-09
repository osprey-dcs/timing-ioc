#!/usr/bin/env python

import argparse
import epics
import math
import sys
import time

parser = argparse.ArgumentParser(description='Set event generator time-of-day',\
         formatter_class=argparse.ArgumentDefaultsHelpFormatter)
parser.add_argument('-p', '--prefix', default='EVG:', help='EVG PV name prefix')
args = parser.parse_args()

def openPV(name):
    return epics.PV(args.prefix + name)

activatePV = openPV('FPGA:activateEVG')
secondsPV = openPV('EVG:setSeconds')

activatePV.put(1)

while True:
    (fraction, seconds) = math.modf(time.time())
    if (fraction < 0.15):
        status = secondsPV.put(seconds, wait=True)
        break
    time.sleep(1.0 - fraction)
print(f"{secondsPV.pvname} <- {int(seconds)}   {status}")
