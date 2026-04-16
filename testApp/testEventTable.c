/*************************************************************************\
* Copyright (c) 2026 Osprey Distributed Control Systems
* SPDX-License-Identifier: BSD
\*************************************************************************/

#define USE_TYPED_RSET

#include <string.h>

#include <testMain.h>
#include <alarm.h>
#include <iocsh.h>
#include <epicsEvent.h>
#include <callback.h>
#include <dbAccess.h>
#include <dbScan.h>
#include <dbStaticLib.h>
#include <dbUnitTest.h>

extern
int testBitTable_registerRecordDeviceDriver(struct dbBase *);

static
int testTIMEeq(const char *pv, epicsUInt32 sec, epicsUInt32 nsec)
{
    dbCommon * prec = testdbRecordPtr(pv);
    epicsTimeStamp ts;
    dbScanLock(prec);
    ts = prec->time;
    dbScanUnlock(prec);

    return testOk(ts.secPastEpoch==sec && ts.nsec==nsec,
                  "%s.TIME (%u, %u) == %u, %u",
                  prec->name,
                  ts.secPastEpoch, ts.nsec,
                  sec, nsec);
}

MAIN(testEventTable)
{
    testPlan(23);

    testdbPrepare();
    testdbReadDatabase("testBitTable.dbd", NULL, NULL);
    testBitTable_registerRecordDeviceDriver(pdbbase);

    testdbReadDatabase("testEventTable.db", NULL, "P=TST:");
    testIocInitOk();

    testdbPutFieldOk("TST:mult", DBF_LONG, 2);

    testdbPutFieldOk("TST:last1.PROC", DBF_LONG, 0);
    testdbGetFieldEqual("TST:last1", DBF_LONG, 0);
    testTIMEeq("TST:last1", 0, 0);

    testDiag("Push nothing");
    {
        const epicsUInt32 evtlog[] = {0, 0, 0, 0, 0, 0};
        testdbPutArrFieldOk("TST:input", DBF_ULONG, NELEMENTS(evtlog), evtlog);
    }
    testSyncCallback();
    testdbGetFieldEqual("TST:last1", DBF_LONG, 0); // no event code
    testTIMEeq("TST:last1", 0, 0);

    testdbPutFieldOk("TST:code1", DBF_LONG, 100);
    testdbPutFieldOk("TST:code2", DBF_LONG, 25);

    testDiag("Push uninteresting");
    {
        const epicsUInt32 evtlog[] = {5, 10, 1, 0, 0, 0, 10, 11, 2, 0, 0, 0};
        testdbPutArrFieldOk("TST:input", DBF_ULONG, NELEMENTS(evtlog), evtlog);
    }
    testSyncCallback();
    testdbGetFieldEqual("TST:last1", DBF_LONG, 0); // nothing happened
    testdbGetFieldEqual("TST:last2", DBF_LONG, 0);
    testTIMEeq("TST:last1", 0, 0);

    testDiag("Push both");
    {
        const epicsUInt32 evtlog[] = {25,631152012,1, 100,631152012,2, 100,631152012,3, 25,631152012,4};
        testdbPutArrFieldOk("TST:input", DBF_ULONG, NELEMENTS(evtlog), evtlog);
    }
    testSyncCallback();
    testdbGetFieldEqual("TST:last1", DBF_LONG, 2);
    testTIMEeq("TST:last1", 12, 3*2); // time of last
    testdbGetFieldEqual("TST:last2", DBF_LONG, 2);
    testTIMEeq("TST:last2", 12, 4*2);

    testTIMEeq("TST:buf1", 12, 2*2); // time of first in buffer
    {
        const double dlt[] = {0, 2e-9};
        testdbGetArrFieldEqual("TST:buf1", DBF_DOUBLE, 5, NELEMENTS(dlt), dlt);
    }

    testDiag("Push only 25");
    {
        const epicsUInt32 evtlog[] = {25,631152012,8};
        testdbPutArrFieldOk("TST:input", DBF_ULONG, NELEMENTS(evtlog), evtlog);
    }
    testSyncCallback();
    testdbGetFieldEqual("TST:last1", DBF_LONG, 2);
    testdbGetFieldEqual("TST:last2", DBF_LONG, 3);

    testIocShutdownOk();
    testdbCleanup();

    return testDone();
}

