/*************************************************************************\
* Copyright (c) 2026 Osprey Distributed Control Systems
* SPDX-License-Identifier: BSD
\*************************************************************************/
/* Calculate settings for Skyworks Si570 synth.
 */
/*
 * Marble SI570 MGT clock reference
 *
 * Must lookup reference frequency and address by part number
 *   https://tools.skyworksinc.com/TimingUtility/timing-part-number-search-results.aspx
 *
 * Marble 1.4.x has 570NBB001808DGR
 *   I2C address: 0x55
 *   Ref. frequency: 270 MHz
 *   Temperature Stability: 20 PPM
 */
/* The Si570 is an odd one.  On reset it reverts to a specified default output frequency,
 * which can not be introspected, but must be looked up from the part number.
 * To compute new outputs, we first have to read back the burned in default settings
 * which achieve this specified output.
 * The datasheet is emphatic that these values can vary from part to part.
 *
 * Not kidding.  Observed two different 570NBB001808DGR
 *   0xa042a8124886
 *   0xa042a842a13e
 */

#include <dbDefs.h>
#include <epicsMath.h>
#include <epicsTypes.h>
#include <aSubRecord.h>
#include <recGbl.h>
#include <alarm.h>
#include <menuFtype.h>
#include <registryFunction.h>
#include <epicsExport.h>

static const struct {
    epicsUInt8 reg, div;
} hsdivs[] = {
    {0, 4},
    {1, 5},
    {2, 6},
    {3, 7},
    {5, 9},
    {7, 11},
};

/*
 * record(aSub, "xxx") {
 *   field(SNAM, "si570Calc")
 *   field(FTA , "DOUBLE") # default/calibration output frequency
 *   field(FTB , "DOUBLE") # desired output frequency
 *   field(FTC , "LONG") # default/calibration register values
 *   field(NOC , "2")
 *   field(FTVA, "LONG")
 *   field(NOVA, "2")
 * }
 */
static
long si570Calc(aSubRecord *prec)
{
    prec->neva = 0; // spoil

    if(prec->fta!=menuFtypeDOUBLE || prec->nea!=1)
        return 1;
    if(prec->ftb!=menuFtypeDOUBLE || prec->neb!=1)
        return 1;
    if(prec->ftc!=menuFtypeLONG || prec->nec!=2)
        return 1;
    if(prec->ftva!=menuFtypeLONG || prec->nova!=2)
        return 1;

    const double f_cal = 1e6 * (*(const double*)prec->a);
    const double f_out = 1e6 * (*(const double*)prec->b);
    const epicsInt32* reg_cal = prec->c;
    epicsInt32* reg_out = prec->vala;

    // decompose default/calibration register values
    epicsUInt8 hsdiv=0, n1;
    double rfreq;
    {
        // 3x 8-bit registers packed into each array element
        epicsUInt32 r7_9 = reg_cal[0];
        epicsUInt32 r10_12 = reg_cal[1];

        epicsUInt8 hsdiv_r = (r7_9 >> 21) & 0x7;
        epicsUInt8 n1_r = (r7_9 >> 14) & 0x7f;
        epicsUInt64 rfreq_r = r7_9 & 0x3fff;
        rfreq_r <<= 24;
        rfreq_r |= (r10_12 & 0xffffff);

        int found = 0;
        for(unsigned i=0; i<NELEMENTS(hsdivs); i++) {
            if(hsdivs[i].reg==hsdiv_r) {
                hsdiv = hsdivs[i].div;
                found = 1;
                break;
            }
        }
        if(!found) {
            recGblSetSevrMsg(prec, CALC_ALARM, MAJOR_ALARM, "bad cal hsdiv");
            return 0;
        }

        n1 = n1_r+1; // 0 is /1

        // 38 bits as 10.28 fixed precision
        rfreq = rfreq_r / (double)0x10000000;
    }

    // derive internal XTAL frequency, the actual calibration
    const double f_xtal = f_cal * n1 * hsdiv / rfreq;
    // spec. is: 114.285 += 2000ppm (aka. 0.2%, or 0.22857 MHz)
    if(f_xtal<113.9e6 || f_xtal>114.7e6) {
        recGblSetSevrMsg(prec, COMM_ALARM, INVALID_ALARM, "XTAL OoR");
        return 0;
    }

    double err = HUGE_VAL;
    epicsUInt8 hsdiv_r = 0;
    epicsUInt8 n1_r = 0;
    epicsUInt64 rfreq_r = 0;

    // exhaustive search
    for(unsigned hs=0; hs<NELEMENTS(hsdivs); hs++) {
        for(unsigned n1=1; n1<=0x80; n1*=2) { // 1, 2, 4, ... 128
            hsdiv = hsdivs[hs].div;
            double f_dco = f_out * n1 * hsdiv;

            if(f_dco < 4.85e9 || f_dco > 5.67e9)
                continue;

            double cand_rfreq = f_dco / f_xtal;

            epicsUInt64 cand_rfreq_r = cand_rfreq * (double)0x10000000;
            if(cand_rfreq_r >= 0x4000000000)
                continue;

            double cand_rfreq2 = cand_rfreq_r / (double)0x10000000; // truncated

            double f_out_o = f_xtal * cand_rfreq2 / hsdiv / n1;
            double cand_err = fabs(f_out_o - f_out);

            if(cand_err > err) // favor larger hsdiv
                continue;

            rfreq_r = cand_rfreq_r;
            n1_r = n1-1;
            hsdiv_r = hsdivs[hs].reg;
            err = cand_err;
        }
    }

    if(!finite(err)) {
        recGblSetSevrMsg(prec, CALC_ALARM, MAJOR_ALARM, "No match");
        return 0;
    }

    /* 0xe00000 - HSDIV
     * 0x1fc000 - N1
     * 0x003fff - RFREQ[37:24]
     */
    epicsUInt32 r7_9 = hsdiv_r<<21 | n1_r<<14 | rfreq_r>>24;
    // 0xffffff - RFREQ[23:0]
    epicsUInt32 r10_12 = rfreq_r&0xffffff;

    reg_out[0] = r7_9;
    reg_out[1] = r10_12;
    prec->neva = 2;

    return 0;
}

epicsRegisterFunction(si570Calc);
