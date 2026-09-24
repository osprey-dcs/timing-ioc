/*************************************************************************\
* Copyright (c) 2026 Osprey Distributed Control Systems
* SPDX-License-Identifier: BSD
\*************************************************************************/
/** EVG sequence table mux
 */

#include <epicsTypes.h>
#include <epicsMath.h>
#include <aSubRecord.h>
#include <recGbl.h>
#include <alarm.h>
#include <epicsMath.h>

#include <registryFunction.h>
#include <epicsExport.h>

/**
 * record(aSub, "blah") {
 *   field(SNAM, "timingSeqMux")
 *   field(FTA , "UCHAR") # event codes
 *   field(NOA , "1024")
 *   field(FTB , "DOUBLE") # time delays (ns)
 *   field(NOB , "1024") # == NOA
 *   field(FTC , "ULONG") # delay field bit width
 *   field(FTD , "DOUBLE") # period (ns)
 *
 *   field(FTVA, "ULONG") # mux.d output array
 *   field(NOVA, "2048") # 2x NOA
 * }
 */
static
long timingSeqMux(aSubRecord *prec)
{
    const epicsUInt8 *codes = prec->a;
    const double *delays = prec->b;
    const epicsUInt32 bitwidth = *(epicsUInt32*)prec->c;
    const double period = *(double*)prec->d;
    epicsUInt32 *out = prec->vala;
    epicsUInt32 N = prec->nea;

    if(bitwidth > 32) {
        recGblSetSevrMsg(prec, WRITE_ALARM, INVALID_ALARM, "bits");
        return -1;
    }
    if(!isfinite(period) || period <= 0.0) {
        recGblSetSevrMsg(prec, WRITE_ALARM, INVALID_ALARM, "period");
        return -1;
    }
    epicsUInt64 maxdelay = ((epicsUInt64)1u)<<bitwidth;

    // silently truncate to shorter column
    if(N > prec->neb)
        N = prec->neb;

    epicsUInt32 o=0;

    epicsUInt64 prevDly=0; // absolute delay of previous input event
    for(epicsUInt32 n=0; n<N; n++) {
        if(!isfinite(delays[n]) || delays[n] < 0.0) {
            recGblSetSevrMsg(prec, WRITE_ALARM, INVALID_ALARM,
                             "OoB @%u", (unsigned)n);
            goto trunc;
        }

        epicsUInt64 dly = ceil(delays[n] / period); // round to later
        if(n!=0) {
            // delay for each row is number of ticks between the two events.
            // A zero delay requests consecutive cycles.
            if(dly <= prevDly) {
                // delay sequence must increase monotonically
                recGblSetSevrMsg(prec, WRITE_ALARM, INVALID_ALARM,
                                 "!mono @%u", (unsigned)n);
                o = 0; // refuse partial sequence
                goto trunc;
            } else {
                epicsUInt64 oldPrev = prevDly;
                prevDly = dly;
                dly -= oldPrev+1;
            }
        }
        // 'dly' is now relative to slot after previous input event

        while(1) {
            if(2*o+1 >= prec->nova) {
                recGblSetSevrMsg(prec, WRITE_ALARM, INVALID_ALARM, "Oflow");
                o = 0;
                goto trunc;
            }

            if(dly >= maxdelay) {
                // insert no-op event to extend delay
                out[2*o+0] = maxdelay-1;
                out[2*o+1] = 0;
                dly -= maxdelay;
                o++;

            } else {
                out[2*o+0] = dly;
                out[2*o+1] = codes[n];
                o++;
                break;
            }
        }
    }
trunc:

    // fill unused with stop code
    while(2*o+1 < prec->nova) {
        out[2*o+0] = 0;
        out[2*o+1] = 255;
        o++;
    }

    prec->neva = prec->nova;

    return 0;
}

epicsRegisterFunction(timingSeqMux);
