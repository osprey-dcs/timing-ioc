/*************************************************************************\
* Copyright (c) 2026 Osprey Distributed Control Systems
* SPDX-License-Identifier: BSD
\*************************************************************************/

#define USE_TYPED_RSET

#include <string.h>

#include <testMain.h>
#include <dbDefs.h>
#include <alarm.h>
#include <iocsh.h>
#include <epicsEvent.h>
#include <callback.h>
#include <dbAccess.h>
#include <dbStaticLib.h>
#include <dbUnitTest.h>

extern
int testBitTable_registerRecordDeviceDriver(struct dbBase *);

MAIN(testSeqMux)
{
    testPlan(13);
    testdbPrepare();
    testdbReadDatabase("testBitTable.dbd", NULL, NULL);
    testBitTable_registerRecordDeviceDriver(pdbbase);

    testdbReadDatabase("testSeqMux.db", NULL, "P=TST:");
    testIocInitOk();

    testdbPutFieldOk("TST:mux.C", DBF_LONG, 8); // bits
    testdbPutFieldOk("TST:mux.D", DBF_LONG, 8); // ns/tick

    {
        const epicsUInt8 codes[]   = {5,   10,  11,  12,   15,     20};
        const epicsUInt32 delays[] = {0*8, 1*8, 3*8, 16*8, 1500*8, 2000*8};
        testdbPutArrFieldOk("TST:mux.A", DBF_UCHAR, NELEMENTS(codes), codes);
        testdbPutArrFieldOk("TST:mux.B", DBF_ULONG, NELEMENTS(delays), delays);
        testdbPutFieldOk("TST:mux.PROC", DBF_LONG, 0);

        const epicsUInt32 expect[] = {
              0, 5,  // @0
              0, 10, // @1
              1, 11, // @3
             12, 12, // @16
            255, 0,
            255, 0,
            255, 0,
            255, 0,
            255, 0,
            203, 15,
            255, 0,
            243, 20,
            0, 255,
            0, 255,
            0, 255,
            0, 255};
        testdbGetArrFieldEqual("TST:mux.VALA", DBF_ULONG, NELEMENTS(expect)+1, NELEMENTS(expect), expect);
        testdbGetFieldEqual("TST:mux.SEVR", DBF_LONG, NO_ALARM);
    }
    {
        const epicsUInt8 codes[] =   {5,   10,   15,   20,  255}; // 5th ignored
        const epicsUInt32 delays[] = {500, 1000, 1500, 0xffffffff}; // 4th overflows
        testdbPutArrFieldOk("TST:mux.A", DBF_UCHAR, NELEMENTS(codes), codes);
        testdbPutArrFieldOk("TST:mux.B", DBF_ULONG, NELEMENTS(delays), delays);
        testdbPutFieldOk("TST:mux.PROC", DBF_LONG, NO_ALARM);

        const epicsUInt32 expect[] = {
            0, 255, 0, 255, 0, 255, 0, 255,
            0, 255, 0, 255, 0, 255, 0, 255,
            0, 255, 0, 255, 0, 255, 0, 255,
            0, 255, 0, 255, 0, 255, 0, 255};
        testdbGetArrFieldEqual("TST:mux.VALA", DBF_ULONG, NELEMENTS(expect)+1, NELEMENTS(expect), expect);
        testdbGetFieldEqual("TST:mux.SEVR", DBF_LONG, INVALID_ALARM); // overflow
#ifdef DBR_AMSG
        testdbGetFieldEqual("TST:mux.AMSG", DBF_STRING, "Oflow");
#else
        testSkip(1, "No AMSG");
#endif
    }

    testIocShutdownOk();
    testdbCleanup();

    return testDone();
}
