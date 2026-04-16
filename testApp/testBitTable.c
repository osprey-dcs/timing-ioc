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
#include <dbStaticLib.h>
#include <dbUnitTest.h>

extern
int testBitTable_registerRecordDeviceDriver(struct dbBase *);

MAIN(testBitTable)
{
    testPlan(27);

    testdbPrepare();
    testdbReadDatabase("testBitTable.dbd", NULL, NULL);
    testBitTable_registerRecordDeviceDriver(pdbbase);

    testdbReadDatabase("testBitTable.db", NULL, "P=TST:");
    testIocInitOk();

    // default to zero bits per word, so empty output
    {
        epicsUInt32 unused;
        testdbGetArrFieldEqual("TST:Tbl-I", DBF_ULONG, 2, 0, &unused);
    }

    testdbPutFieldFail(-1, "TST:NBits-SP", DBF_LONG, 0); // out of range

    testdbPutFieldOk("TST:NBits-SP", DBF_LONG, 4);
    testSyncCallback();

    {
        epicsUInt32 expected[256];
        memset(expected, 0, sizeof(expected));
        epicsUInt32 actual[256];
        memset(actual, 0, sizeof(actual));
        testdbGetArrFieldEqual("TST:Tbl-I", DBF_ULONG, 257, NELEMENTS(actual), &actual);
        testdbGetFieldEqual("TST:Tbl-I.SEVR", DBF_LONG, 0);
        testOk1(memcmp(actual, expected, sizeof(expected))==0);
    }

    testdbPutFieldOk("TST:Action0_0-SP", DBF_LONG, 100);
    testdbPutFieldOk("TST:Action0_1-SP", DBF_LONG, 255);
    testdbPutFieldOk("TST:Action3_0-SP", DBF_LONG, 100);
    testSyncCallback();
    testOk1(!iocshCmd("dbior \"\" 1"));
    {
        epicsUInt32 expected[256];
        memset(expected, 0, sizeof(expected));
        expected[100] = 0x9;
        expected[255] = 0x1;

        testdbGetArrFieldEqual("TST:Tbl-I", DBF_ULONG, 257, NELEMENTS(expected), &expected);
        testdbGetFieldEqual("TST:Tbl-I.SEVR", DBF_LONG, 0);
    }

    // duplicate not allowed, prev mapping cleared
    testdbPutFieldFail(-1, "TST:Action0_1-SP", DBF_LONG, 100);

    testdbPutFieldOk("TST:Action15_0-SP", DBF_LONG, 100); // out of bounds
    testdbPutFieldOk("TST:Action39_0-SP", DBF_LONG, 100); // out of bounds
    testSyncCallback();
    testOk1(!iocshCmd("dbior \"\" 1"));
    {
        epicsUInt32 expected[256];
        memset(expected, 0, sizeof(expected));
        expected[100] = 0x9;
        // 255 mapping cleared on error

        testdbGetArrFieldEqual("TST:Tbl-I", DBF_ULONG, NELEMENTS(expected), NELEMENTS(expected), &expected);
        testdbGetFieldEqual("TST:Tbl-I.SEVR", DBF_LONG, MAJOR_ALARM); // some out of bounds
    }

    testdbPutFieldOk("TST:Action0_1-SP", DBF_LONG, 255);
    testdbPutFieldOk("TST:NBits-SP", DBF_LONG, 16);
    testSyncCallback();
    testOk1(!iocshCmd("dbior \"\" 1"));
    {
        epicsUInt32 expected[256];
        memset(expected, 0, sizeof(expected));
        expected[100] = 0x8009;
        expected[255] = 0x1;

        testdbGetArrFieldEqual("TST:Tbl-I", DBF_ULONG, 257, NELEMENTS(expected), &expected);
        testdbGetFieldEqual("TST:Tbl-I.SEVR", DBF_LONG, MAJOR_ALARM); // some out of bounds
    }

    testdbPutFieldOk("TST:NBits-SP", DBF_LONG, 40);
    testSyncCallback();
    testOk1(!iocshCmd("dbior \"\" 1"));
    {
        epicsUInt32 expected[512];
        memset(expected, 0, sizeof(expected));
        expected[2*100+0] = 0x0080; // high word first
        expected[2*100+1] = 0x8009;
        expected[2*255+1] = 0x1;

        testdbGetArrFieldEqual("TST:Tbl-I", DBF_ULONG, NELEMENTS(expected), NELEMENTS(expected), &expected);
        testdbGetFieldEqual("TST:Tbl-I.SEVR", DBF_LONG, 0); // all in range
    }

    testIocShutdownOk();
    testdbCleanup();

    return testDone();
}
