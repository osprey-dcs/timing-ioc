/*************************************************************************\
* Copyright (c) 2026 Osprey Distributed Control Systems
* SPDX-License-Identifier: BSD
\*************************************************************************/

#define USE_TYPED_RSET

#include <testMain.h>
#include <alarm.h>
#include <iocsh.h>
#include <epicsEvent.h>
#include <callback.h>
#include <dbAccess.h>
#include <dbStaticLib.h>
#include <dbUnitTest.h>

extern
int testBitTable_registerRecordDeviceDriver(struct dbBase *);

MAIN(testsi570Calc)
{
    testPlan(25);

    testdbPrepare();
    testdbReadDatabase("testBitTable.dbd", NULL, NULL);
    testBitTable_registerRecordDeviceDriver(pdbbase);

    testdbReadDatabase("testsi570Calc.db", NULL, "P=TST:");
    testIocInitOk();

    testDiag("datasheet example (sec. 3.1.2.1)");
    {
        testdbPutFieldOk("TST:si570.A", DBF_DOUBLE, 156.25); // current
        testdbPutFieldOk("TST:si570.B", DBF_DOUBLE, 161.1328125); // desired
        const epicsInt32 cal[2] = {0x1c2bc, 0x011eb8};
        // hsdiv=4, n1=8, rfreq=43.7502734363
        testdbPutArrFieldOk("TST:si570.C", DBF_LONG, NELEMENTS(cal), cal);
        testdbPutFieldOk("TST:si570.PROC", DBF_LONG, 1);

        // this example calculation rounds 12111128493.429882 down to 12111128493

        // hsdiv=4, n1=8, rfreq=45.11746948
        const epicsInt32 expect[] = {0x1c2d1, 0xe127ad};
        testdbGetArrFieldEqual("TST:si570.VALA", DBF_ULONG, NELEMENTS(expect)+1, NELEMENTS(expect), expect);
    }

    testDiag("datasheet example (sec. 3.2)");
    {
        testdbPutFieldOk("TST:si570.A", DBF_DOUBLE, 161.1328125); // current
        testdbPutFieldOk("TST:si570.B", DBF_DOUBLE, 161.132812); // desired
        const epicsInt32 cal[2] = {0x1c2d1, 0xe127ad};
        // hsdiv=4, n1=8, rfreq=45.11746948
        testdbPutArrFieldOk("TST:si570.C", DBF_LONG, NELEMENTS(cal), cal);
        testdbPutFieldOk("TST:si570.PROC", DBF_LONG, 1);

        // this example calculation rounds 12111128455.4188 up to 12111128456   oops!!!

        // hsdiv=4, n1=8, rfreq=45.11746934
        // datasheet shows: 0x1c2d1, 0xe12788
        // due to inconsistant rounding, off-by-one
        const epicsInt32 expect[] = {0x1c2d1, 0xe12787};
        testdbGetArrFieldEqual("TST:si570.VALA", DBF_ULONG, NELEMENTS(expect)+1, NELEMENTS(expect), expect);
    }

    testDiag("round trip of observed calib of a 570NBB001808DGR");
    // f_xtal = 114340932.1636232
    {
        testdbPutFieldOk("TST:si570.A", DBF_DOUBLE, 270.0); // ref.
        testdbPutFieldOk("TST:si570.B", DBF_DOUBLE, 270.0); // desired
        const epicsInt32 cal[2] = {0xa042a8, 0x124886};
        // hsdiv=9, n1=2, rfreq=42.504463694989681
        testdbPutArrFieldOk("TST:si570.C", DBF_LONG, NELEMENTS(cal), cal);
        testdbPutFieldOk("TST:si570.PROC", DBF_LONG, 1);

        const epicsInt32 expect[] = {0xa042a8, 0x124886};
        testdbGetArrFieldEqual("TST:si570.VALA", DBF_ULONG, NELEMENTS(expect)+1, NELEMENTS(expect), expect);
    }

    testDiag("observed calib for 125 MHz");
    {
        testdbPutFieldOk("TST:si570.A", DBF_DOUBLE, 270.0); // ref.
        testdbPutFieldOk("TST:si570.B", DBF_DOUBLE, 125.0); // desired
        const epicsInt32 cal[2] = {0xa042a8, 0x124886};
        // hsdiv=9, n1=2, rfreq=42.504463694989681
        testdbPutArrFieldOk("TST:si570.C", DBF_LONG, NELEMENTS(cal), cal);
        testdbPutFieldOk("TST:si570.PROC", DBF_LONG, 1);

        // hsdiv=5, n1=8, rfreq=43.728872112929821
        const epicsInt32 expect[] = {0x21c2bb, 0xa975ce};
        testdbGetArrFieldEqual("TST:si570.VALA", DBF_ULONG, NELEMENTS(expect)+1, NELEMENTS(expect), expect);
    }

    testDiag("observed calib for APS RF");
    {
        testdbPutFieldOk("TST:si570.A", DBF_DOUBLE, 270.0); // ref.
        testdbPutFieldOk("TST:si570.B", DBF_DOUBLE, 117.313); // desired
        const epicsInt32 cal[2] = {0xa042a8, 0x124886};
        // hsdiv=9, n1=2, rfreq=42.504463694989681
        testdbPutArrFieldOk("TST:si570.C", DBF_LONG, NELEMENTS(cal), cal);
        testdbPutFieldOk("TST:si570.PROC", DBF_LONG, 1);

        // hsdiv=6, n1=8, rfreq=49.247665673494339
        const epicsInt32 expect[] = {0x41c313, 0xf67048};
        testdbGetArrFieldEqual("TST:si570.VALA", DBF_ULONG, NELEMENTS(expect)+1, NELEMENTS(expect), expect);
    }

    testIocShutdownOk();
    testdbCleanup();

    return testDone();
}
