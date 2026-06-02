#!/usr/bin/env python

import argparse
import epics
import sys
import time

# FIXME -- should allow testing of MPS outputs other than first
parser = argparse.ArgumentParser(description='Test MPS', \
         formatter_class=argparse.ArgumentDefaultsHelpFormatter, \
         epilog="""If the MPS mitigation outputs are being checked it is
                 assumed that the EVG and fanout 'MPS:required' records
                 are correct.  If one or more of these records is incorrect
                 you'll likely get complaints about mitigation outputs behaving
                 imroperly.""")
parser.add_argument('-e', '--evg', default='EVG:', help='EVG PV name prefix')
parser.add_argument('-i', '--inputs', default=8, type=int, choices=range(1,9), help='Number of MPS inputs')
parser.add_argument('-m', '--mitigate', action='store_true', help='Check EVG MPS mitigation outputs')
parser.add_argument('-o', '--output', default=1, type=int, choices=range(1,9), help='MPS output number')
parser.add_argument('-r', '--evr', default='EVR:1:', help='EVR PV name prefix')
parser.add_argument('-v', '--verbose', action='store_true', help='Show channel access actions')
args = parser.parse_args()
outputStr = '%d' % args.output
outputBit = 1 << (args.output - 1)
mpsClearEventCode = 126
mpsInputMask = (1 << args.inputs) - 1
exitStatus = 0

#
# Open connection to event generator (MPS central node)
#
def openEVG(name):
    pv = epics.PV(args.evg + name)
    return pv

#
# Open connection to unit under test
#
def openTest(name):
    pv = epics.PV(args.evr + name)
    return pv

#
# Open connection to test node and save value for later restoration
#
restoreList = [];
def openAndStash(name):
    pv = openTest(name)
    val = pv.get()
    restoreList.append([pv, val])
    return pv

def caget(pv):
    val = pv.get()
    if (val == None):
        raise OSError(f"Can't get {pv.pvname}")
    if (args.verbose): print(f'caget {pv.pvname} {val}')
    return val

def caput(pv, val):
    if (args.verbose): print(f'caput {pv.pvname} {val}')
    pv.put(val, wait=True)


# Check MPS status
def checkStatus(expectStatus, \
               expectFirstInputs=None, \
               expectFirstSeconds=None, \
               expectFirstTicks=None):
    failed = False
    caput(statusPV_PROC, 1)
    time.sleep(0.1)
    if (args.mitigate): caput(trippedPV_PROC, 1)
    status = caget(statusPV) & 0xFFFFFFFF
    if (args.mitigate): tripped = (caget(trippedPV) & outputBit) != 0
    expectTripped = (status & 0x1) != 0
    print("Status:%06X" % (status), end='')
    if (status != expectStatus):
        print(" Expect:%06X" % (expectStatus), end='')
        failed = True
    if (expectFirstInputs != None):
        caput(firstInputsPV_PROC, 1)
        caput(firstSecondsPV_PROC, 1)
        caput(firstTicksPV_PROC, 1)
        firstInputs = caget(firstInputsPV)
        firstSeconds = caget(firstSecondsPV)
        firstTicks = caget(firstTicksPV)
        print("  First fault inputs:%X seconds:%d ticks:%d" % \
                                (firstInputs, firstSeconds, firstTicks), end='')
        if (firstInputs != expectFirstInputs):
          print("--Expect:%X" % (expectFirstInputs), end='')
          failed = True
        if (expectFirstSeconds != None):
            if (firstSeconds != expectFirstSeconds): failed = True
            if (firstTicks != expectFirstTicks): failed = True
    if (args.mitigate and (tripped != expectTripped)):
        print(" -- Mitigation output is%s asserted." % \
                                            ("" if tripped else " not"), end='')
        failed = True
    print(" -- %s" % ("FAIL" if failed else "PASS"), end='')
    print("")
    if (failed): raise(Exception("MPS status"))
    
invertPV = openAndStash('MPS:invert')
goodStatePV = openAndStash('MPS:goodState:%s' % (outputStr))
chkInputsPV = openAndStash('MPS:chkInputs:%s' % (outputStr))

statusPV = openTest('MPS:status:%s' % (outputStr))
statusPV_PROC = openTest('MPS:status:%s.PROC' % (outputStr))
firstInputsPV = openTest('MPS:firstInputs:%s' % (outputStr))
firstInputsPV_PROC = openTest('MPS:firstInputs:%s.PROC' % (outputStr))
firstSecondsPV = openTest('MPS:firstSeconds:%s' % (outputStr))
firstSecondsPV_PROC = openTest('MPS:firstSeconds:%s.PROC' % (outputStr))
firstTicksPV = openTest('MPS:firstTicks:%s' % (outputStr))
firstTicksPV_PROC = openTest('MPS:firstTicks:%s.PROC' % (outputStr))
forceTripPV = openTest('MPS:forceTrip')

clearPV = openEVG('EVG:swEvent') 
if (args.mitigate):
    trippedPV = openEVG('MPS:tripped')
    trippedPV_PROC = openEVG('MPS:tripped.PROC')

try:
    ########################################################################
    # Attemp to clear any outstanding trips
    caput(forceTripPV, 0)
    caput(clearPV, mpsClearEventCode)
    checkStatus(0)

    ########################################################################
    # Ensure that a 'force trip' has the desired result
    caput(forceTripPV, outputBit)
    checkStatus(0x80000003)

    ########################################################################
    # Ensure that forced trip can be cleared
    caput(forceTripPV, 0)
    caput(clearPV, mpsClearEventCode)
    checkStatus(0)

    for inputIndex in range(0,args.inputs):
        inputBit = 1 << inputIndex
        ########################################################################
        # Prepare for test
        # Make all inputs on the test node 'unimportant'
        caput(chkInputsPV, mpsInputMask)

        # Make all inputs look low by inverting those that now look high
        inverted = caget(invertPV)
        inputState = (caget(statusPV) >> 16) & mpsInputMask
        inverted ^= inputState

        # Make 0 the good state for all inputs
        caput(goodStatePV, 0)

        # Make all inputs on the test node 'important'
        caput(chkInputsPV, mpsInputMask)

        # Clear any leftover trips
        caput(clearPV, mpsClearEventCode)

        #######################################################################
        # Ensure that preliminary state is correct
        checkStatus(0)

        ########################################################################
        # Ensure that an important input going bad causes a trip
        caput(invertPV, inverted ^ inputBit)
        checkStatus((inputBit << 16) | 0x3, inputBit)

        ########################################################################
        # Ensure that important input going good removes fault but not the trip
        caput(invertPV, inverted)
        checkStatus(0x00001, inputBit)

        ########################################################################
        # Ensure that an input going bad causes fault but leaves 
        # first trip values unchanged
        firstTripInputs = caget(firstInputsPV)
        firstTripSeconds = caget(firstSecondsPV)
        firstTripTicks = caget(firstTicksPV)
        for i in range(0,args.inputs):
            b = 1 << i
            caput(invertPV, inverted ^ b)
            checkStatus((b << 16) | 0x3, firstTripInputs, firstTripSeconds, firstTripTicks)

            ####################################################################
            # Ensure that input going good removes fault but not the trip
            caput(invertPV, inverted)
            checkStatus(0x1, firstTripInputs, firstTripSeconds, firstTripTicks)

        ########################################################################
        # Ensure that trip clear has the desired effect
        caput(clearPV, mpsClearEventCode)
        checkStatus(0)

except BaseException as e:
    print({e}, file=sys.stderr)
    exitStatus = 1
finally:
    for l in  restoreList:
        l[0].put(l[1])
print("PASS" if exitStatus == 0 else "FAIL")
sys.exit(exitStatus)
