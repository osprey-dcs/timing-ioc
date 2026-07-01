/*************************************************************************\
* Copyright (c) 2026 Osprey Distributed Control Systems
* SPDX-License-Identifier: BSD
\*************************************************************************/

#include <atomic>
#include <stdexcept>

#include <time.h>
#include <pthread.h>

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
    bool running{true};
    std::atomic<uint32_t> nextSec{0};
    pthread_mutex_t ll;
    pthread_cond_t cc;
    epicsThread worker;

    secondsGbl()
        :ll(PTHREAD_MUTEX_INITIALIZER)
        ,cc(PTHREAD_COND_INITIALIZER)
        ,worker(*this, "secondsTick", 0)
    {
        scanIoInit(&onSec);
    }

    ~secondsGbl() {
        (void)pthread_cond_destroy(&cc);
        (void)pthread_mutex_destroy(&ll);
    }

    virtual void run() override final {
        if(pthread_mutex_lock(&ll))
            throw std::runtime_error("pthread_mutex_lock");

        while(running) {
            struct timespec until{};

            if(clock_gettime(CLOCK_REALTIME, &until))
                throw std::runtime_error("secondsGbl clock_gettime errors");

            // wait until start of next second
            until.tv_nsec = 0;
            until.tv_sec++;

            auto ret(pthread_cond_timedwait(&cc, &ll, &until));
            if(ret==ETIMEDOUT) {
                // expected
            } else if(!ret || ret==EINTR) {
                // interrupted, or shutdown
                continue;
            } else {
                errlogPrintf("%s pthread_cond_timedwait error %d\n", __FILE__, ret);
                continue; // try again
            }

            nextSec = until.tv_sec+1; // send next next second

            scanIoImmediate(onSec, priorityHigh);
            scanIoImmediate(onSec, priorityMedium);
            scanIoImmediate(onSec, priorityLow);
        }

        if(pthread_mutex_unlock(&ll))
            throw std::runtime_error("pthread_mutex_lock");
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
        prec->time.secPastEpoch = prec->val - POSIX_TIME_AT_EPICS_EPOCH;
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

void secondsFree(void*) noexcept
{
    try {
        delete secGbl;
        secGbl = nullptr;

    } catch(std::exception& e) {
        fprintf(stderr, ERL_ERROR ": %s : %s\n", __func__, e.what());
        return;
    }
}

void secondsShutdown(void*) noexcept
{
    try {
        (void)pthread_mutex_lock(&secGbl->ll);
        secGbl->running = false;
        (void)pthread_cond_signal(&secGbl->cc);
        (void)pthread_mutex_unlock(&secGbl->ll);
        secGbl->worker.exitWait();

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
        secGbl->worker.start();

    } catch(std::exception& e) {
        fprintf(stderr, ERL_ERROR ": %s : %s\n", __func__, e.what());
        return;
    }
    epicsAtExit(secondsShutdown, NULL);

}

void secondsPhaseRegistrar() noexcept
{
    try {
        secGbl = new secondsGbl();

    } catch(std::exception& e) {
        fprintf(stderr, ERL_ERROR ": %s : %s\n", __func__, e.what());
        return;
    }
    epicsAtExit(secondsFree, NULL);
    (void)initHookRegister(secondsInit);
}

} // namespace

extern "C" {
epicsExportRegistrar(secondsPhaseRegistrar);
epicsExportAddress(dset, devLISecPhaseNext);
}
