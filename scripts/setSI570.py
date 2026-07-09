#!/usr/bin/env python

import argparse
import epics
import math
import sys
import time

parser = argparse.ArgumentParser(description='Set SI570 oscillator',\
         formatter_class=argparse.ArgumentDefaultsHelpFormatter)
parser.add_argument('-b', '--fbase', default=270.0, help='Factory Fout (MHz)')
parser.add_argument('-f', '--freq', type=float, default=125.0, help='Desired Fout (MHz)')
parser.add_argument('-p', '--prefix', default='EVG:', help='EVG PV name prefix')
parser.add_argument('-r', '--readonly', action='store_true', help='Read current settings')
parser.add_argument('-v', '--verbose', action='store_true', help='Show channel access operations')
args = parser.parse_args()

def openPV(name):
    return epics.PV(args.prefix + name)

def caget(pv):
    val = pv.get()
    if (val == None):
        raise OSError(f"Can't get {pv.pvname}")
    if (args.verbose): print(f'caget {pv.pvname} {val}')
    return val

def caput(pv, val):
    if (args.verbose): print(f'caput {pv.pvname} {val}')
    pv.put(val, wait=True)

def calculate_si570_params(f_out_mhz, f_xtal_mhz=114.285):
    """
    Computes Si570 parameters for a given target output frequency.
    
    Formula: F_out = (F_xtal * RFREQ) / (N1 * HS_DIV)
    Constraint: 4.85 GHz <= F_dco <= 5.67 GHz
    
    Returns:
        tuple: (hsdiv, n1, rfreq_float, hex_bytes) or None if impossible.
    """
    valid_hsdiv = [4, 5, 6, 7, 9, 11]
    
    best_config = None
    min_error = float('inf')

    # Iterate through valid HS_DIV values
    for hsdiv in valid_hsdiv:
        # Iterate through valid N1 values (N1 = 1 or even integers 2 to 128)
        n1_values = [1] + list(range(2, 129, 2))
        for n1 in n1_values:
            # Calculate required DCO frequency in MHz
            # F_dco = F_out * HS_DIV * N1
            f_dco_mhz = f_out_mhz * hsdiv * n1
            
            # Check DCO constraint (4.85 GHz to 5.67 GHz)
            if 4850.0 <= f_dco_mhz <= 5670.0:
                # RFREQ = F_dco / F_xtal
                rfreq = f_dco_mhz / f_xtal_mhz
                
                # Check for closest match (in case of multiple valid options)
                f_calculated = f_dco_mhz / (hsdiv * n1)
                error = abs(f_calculated - f_out_mhz)
                
                if error < min_error:
                    min_error = error
                    best_config = (hsdiv, n1, rfreq)

    if not best_config:
        raise OSError("Can't find valid HSDIV and N1")
    return best_config


si570r7_9rbk = openPV('MARBLE:SI570:R7_9rbk')
si570r7_9rbk_PROC = openPV('MARBLE:SI570:R7_9rbk.PROC')
si570r10_12rbk = openPV('MARBLE:SI570:R10_12rbk')
si570r10_12rbk_PROC = openPV('MARBLE:SI570:R10_12rbk.PROC')
si570r7_9 = openPV('MARBLE:SI570:R7_9')
si570r10_12 = openPV('MARBLE:SI570:R10_12')
si570r135 = openPV('MARBLE:SI570:R135')
si570r137 = openPV('MARBLE:SI570:R137')


# Recall settings and read them back
if not args.readonly: caput(si570r135, 1)
caput(si570r7_9rbk_PROC, 1)
caput(si570r10_12rbk_PROC, 1)
time.sleep(0.1)
r7_9 = caget(si570r7_9rbk)
r10_12 = caget(si570r10_12rbk)
print("%X %X" % (r7_9, r10_12))

# Extract values from registers
hsdiv_r = (r7_9 >> 21) & 0x7
n1_r = (r7_9 >> 14) & 0x7F
rfreq_r = ((r7_9 & 0x3FFF) << 24) | r10_12;
rfreq_f = float(rfreq_r) / (1 << 28)

# Map some values
hsdiv = (4, 5, 6, 7, 0, 9, 0, 11)[hsdiv_r]
if (hsdiv == 0):
    raise OSError("Invalid HS_DIV value")
n1 = n1_r + 1

Fxtal = args.fbase * hsdiv * n1 / rfreq_f
print("Fxtal %g MHz" % (Fxtal))
if ((Fxtal < 114.275) or (Fxtal > 114.305)):
    raise OSError(f"Invalid Fxtal ({Fxtal})")
    sys.exit(1)

if args.readonly:
    Fout = Fxtal * rfreq_f / (hsdiv * n1)
    print(f"Fout {Fout} ", end="")
else:
    print("Base -- Fout:%g MHz  HSDIV:%d (REG HS_DIV:%X)  N1:%d (REG N1:%d)  RFREQ:%g %X" \
                     % (args.fbase, hsdiv, hsdiv_r, n1, n1_r, rfreq_f, rfreq_r))
    hsdiv, n1, rfreq_f = calculate_si570_params(args.freq, Fxtal)
    hsdiv_r = (-1, -1, -1, -1, 0, 1, 2, 3, -1, 5, -1, 7)[hsdiv]
    n1_r = n1 - 1
    rfreq_r = int(rfreq_f * (1 << 28))

print("HSDIV:%d (REG HS_DIV:%x)  N1:%d (REG N1:%d)  RFREQ:%g %X" % (hsdiv, hsdiv_r, n1, n1_r, rfreq_f, rfreq_r))

if not args.readonly:
    r7_9 = (hsdiv_r << 21) | (n1_r << 14) | (rfreq_r >> 24)
    r10_12 = rfreq_r & 0xFFFFFF
    caput(si570r7_9, r7_9)
    caput(si570r10_12, r10_12)
    caput (si570r137, 0)
