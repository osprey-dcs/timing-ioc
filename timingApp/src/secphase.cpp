/*************************************************************************\
* Copyright (c) 2026 Osprey Distributed Control Systems
* SPDX-License-Identifier: BSD
\*************************************************************************/

#include <atomic>
#include <stdexcept>

#include <time.h>

#define USE_TYPED_DRVET
#define USE_TYPED_RSET
#define USE_TYPED_DSET

#include <epicsStdio.h>
#include <errlog.h>
#include <epicsMutex.h>
#include <epicsGuard.h>
#include <epicsThread.h>
#include <epicsExit.h>
#include <epicsTime.h>

#include <alarm.h>
#include <callback.h>
#include <dbScan.h>
#include <recGbl.h>
#include <initHooks.h>
#include <longinRecord.h>

#include <epicsExport.h>

namespace {

typedef epicsGuard<epicsMutex> Guard;

struct secondsGbl final : private epicsThreadRunable {
    IOSCANPVT onSec;
    std::atomic<bool> running{true};
    std::atomic<uint32_t> nextSec{0};
    epicsThread worker;

    secondsGbl()
        :worker(*this, "secondsTick", 0)
    {
        scanIoInit(&onSec);
    }

    ~secondsGbl() {
        running = false;
        worker.exitWait();
    }

    virtual void run() override final {
        while(running) {
            struct timespec until{};

            if(clock_gettime(CLOCK_REALTIME, &until))
                throw std::runtime_error("secondsGbl clock_gettime errors");

            // wait until start of next second
            until.tv_nsec = 0;
            until.tv_sec++;

            if(clock_nanosleep(CLOCK_REALTIME,
                                TIMER_ABSTIME,
                                &until,
                                NULL))
                throw std::runtime_error("secondsGbl clock_nanosleep errors");

            nextSec = until.tv_sec+1; // send next next second

            scanIoImmediate(onSec, priorityHigh);
            scanIoImmediate(onSec, priorityMedium);
            scanIoImmediate(onSec, priorityLow);
        }
    }
} *secGbl = nullptr;

long secondsGetIoIntr(int detach, struct dbCommon *prec, IOSCANPVT* pscan) noexcept
{
    (void)detach;
    (void)prec;
    if(secGbl)
        *pscan = secGbl->onSec;
    return 0;
}

long secondsGetNextNext(longinRecord *prec)
{
    if(!secGbl) {
        recGblSetSevr(prec, READ_ALARM, INVALID_ALARM);
        return 0;
    }
    prec->val = secGbl->nextSec;
    if(prec->tse == epicsTimeEventDeviceTime) {
        prec->time.secPastEpoch = secGbl->nextSec - POSIX_TIME_AT_EPICS_EPOCH;
        prec->time.nsec = 0;
    }
    return 0;
}

longindset devLISecPhaseNext = {
    {
        5,
        NULL,
        NULL,
        NULL,
        secondsGetIoIntr,
    },
    secondsGetNextNext,
};

void secondsShutdown(void*) noexcept
{
    try {
        secGbl->running = false;
        secGbl->worker.exitWait();
        delete secGbl;
        secGbl = nullptr;

    } catch(std::exception& e) {
        fprintf(stderr, ERL_ERROR ": %s : %s\n", __func__, e.what());
        return;
    }
}

void secondsInit(initHookState state) noexcept
{
    if(state!=initHookAfterIocBuilt)
        return;

    try {
        secGbl = new secondsGbl();
        secGbl->worker.start();

    } catch(std::exception& e) {
        fprintf(stderr, ERL_ERROR ": %s : %s\n", __func__, e.what());
        return;
    }

    epicsAtExit(secondsShutdown, NULL);
}

void secondsPhaseRegistrar() noexcept
{
    (void)initHookRegister(secondsInit);
}

} // namespace

extern "C" {
epicsExportRegistrar(secondsPhaseRegistrar);
epicsExportAddress(dset, devLISecPhaseNext);
}
