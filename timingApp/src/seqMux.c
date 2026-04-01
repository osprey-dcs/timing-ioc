/*************************************************************************\
* Copyright (c) 2026 Osprey Distributed Control Systems
* SPDX-License-Identifier: BSD
\*************************************************************************/
/** EVG sequence table mux
 */

#include <epicsTypes.h>
#include <aSubRecord.h>
#include <recGbl.h>
#include <alarm.h>

#include <registryFunction.h>
#include <epicsExport.h>

/**
 * record(aSub, "blah") {
 *   field(SNAM, "timingSeqMux")
 *   field(FTA , "UCHAR") # event codes
 *   field(NOA , "1024")
 *   field(FTB , "ULONG") # time delays (ns)
 *   field(NOB , "1024") # == NOA
 *   field(FTC , "ULONG") # delay field bit width
 *
 *   field(FTVA, "ULONG") # mux.d output array
 *   field(NOVA, "2048") # 2x NOA
 * }
 */
static
long timingSeqMux(aSubRecord *prec)
{
    const epicsUInt8 *codes = prec->a;
    const epicsUInt32 *delays = prec->b;
    const epicsUInt32 *bitwidth = prec->c;
    epicsUInt32 *out = prec->vala;
    epicsUInt32 N = prec->nea;

    if(*bitwidth > 32) {
        recGblSetSevrMsg(prec, WRITE_ALARM, INVALID_ALARM,
                         "too many bits %u", (unsigned)*bitwidth);
        return -1;
    }
    epicsUInt32 maxdelay = (((epicsUInt64)1u)<<*bitwidth)-1;

    if(N > prec->neb)
        N = prec->neb;

    if(2*N > prec->nova) {
        N = prec->nova/2u;
        recGblSetSevrMsg(prec, WRITE_ALARM, INVALID_ALARM, "Trunc");
    }

    for(epicsUInt32 n=0; n<N; n++) {
        epicsUInt32 dly = delays[n];
        if(dly>maxdelay) {
            recGblSetSevrMsg(prec, WRITE_ALARM, INVALID_ALARM,
                             "oflow @%u", (unsigned)n);
            dly = maxdelay;
        }
        out[2*n+0] = codes[n];
        out[2*n+1] = dly;
    }

    prec->neva = 2*N;

    return 0;
}

epicsRegisterFunction(timingSeqMux);
