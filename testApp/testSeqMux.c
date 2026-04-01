/*************************************************************************\
* Copyright (c) 2026 Osprey Distributed Control Systems
* SPDX-License-Identifier: BSD
\*************************************************************************/

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
    testPlan(11);
    testdbPrepare();
    testdbReadDatabase("testBitTable.dbd", NULL, NULL);
    testBitTable_registerRecordDeviceDriver(pdbbase);

    testdbReadDatabase("testSeqMux.db", NULL, "P=TST:");
    testIocInitOk();

    testdbPutFieldOk("TST:mux.C", DBF_LONG, 12); // bits

    {
        const epicsUInt8 codes[] = {5, 10, 15, 20};
        const epicsUInt32 delays[] = {500, 1000, 1500, 2000};
        testdbPutArrFieldOk("TST:mux.A", DBF_UCHAR, NELEMENTS(codes), codes);
        testdbPutArrFieldOk("TST:mux.B", DBF_ULONG, NELEMENTS(delays), delays);
        testdbPutFieldOk("TST:mux.PROC", DBF_LONG, 0);

        const epicsUInt32 expect[] = {5, 500, 10, 1000, 15, 1500, 20, 2000};
        testdbGetArrFieldEqual("TST:mux.VALA", DBF_ULONG, NELEMENTS(expect)+1, NELEMENTS(expect), expect);
        testdbGetFieldEqual("TST:mux.SEVR", DBF_LONG, NO_ALARM);
    }
    {
        const epicsUInt8 codes[] = {5, 10, 15, 20, 255}; // 5th ignored
        const epicsUInt32 delays[] = {500, 1000, 1500, 0xffffffff}; // 4th overflows
        testdbPutArrFieldOk("TST:mux.A", DBF_UCHAR, NELEMENTS(codes), codes);
        testdbPutArrFieldOk("TST:mux.B", DBF_ULONG, NELEMENTS(delays), delays);
        testdbPutFieldOk("TST:mux.PROC", DBF_LONG, NO_ALARM);

        const epicsUInt32 expect[] = {5, 500, 10, 1000, 15, 1500, 20, 0xfff};
        testdbGetArrFieldEqual("TST:mux.VALA", DBF_ULONG, NELEMENTS(expect)+1, NELEMENTS(expect), expect);
        testdbGetFieldEqual("TST:mux.SEVR", DBF_LONG, INVALID_ALARM); // overflow
    }

    testIocShutdownOk();
    testdbCleanup();

    return testDone();
}
