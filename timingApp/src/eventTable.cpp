/*************************************************************************\
* Copyright (c) 2026 Osprey Distributed Control Systems
* SPDX-License-Identifier: BSD
\*************************************************************************/
/* Event RX table de-mux
 *
 * Inputs:
 *   - event log as sequence of 3x word triples (event, sec, ticks)
 *   - sec/tick scale
 *   - Selection of event codes
 * Output:
 *   - RX count (ai)
 *   - RX buffer (aai)
 */

#include <map>
#include <string>
#include <memory>
#include <stdexcept>
#include <list>

#include <stdint.h>
#include <string.h>

#define USE_TYPED_DRVET
#define USE_TYPED_RSET
#define USE_TYPED_DSET

#include <epicsTypes.h>
#include <epicsMutex.h>
#include <epicsGuard.h>
#include <epicsString.h>
#include <epicsStdio.h>
#include <epicsTime.h>
#include <epicsMath.h>
#include <errlog.h>

#include <alarm.h>
#include <callback.h>
#include <dbAccess.h>
#include <dbScan.h>
#include <dbLock.h>
#include <devSup.h>
#include <drvSup.h>
#include <recGbl.h>
#include <dbCommon.h>
#include <aoRecord.h>
#include <aaoRecord.h>
#include <aaiRecord.h>
#include <longoutRecord.h>
#include <longinRecord.h>
#include <menuFtype.h>

#include <epicsExport.h>


namespace {

typedef epicsGuard<epicsMutex> Guard;

struct EventLog; // entry for mux'd input event log
struct EventQueue; // collection for demux'd for one event code
struct EventDev; // operations

epicsMutex eventLogsLock;
std::map<std::string, std::unique_ptr<EventLog>> eventLogs;

struct EventLog {
    const std::string name;

    epicsMutex lock;
    uint32_t nOverflows=0u;
    double nsecPerTick = 1.0;

    std::map<std::string, std::unique_ptr<EventQueue>> queues;
    std::multimap<uint8_t, EventQueue*> listeners;

    explicit
        EventLog(const std::string& name)
        :name(name)
    {}
};

struct EventQueue {
    EventLog* const log;

    std::list<epicsTime> unused, que;
    epicsTime last;
    IOSCANPVT onChange;

    uint32_t nOccur=0u;
    uint32_t nLimit=0u;
    uint8_t event=0u;
    unsigned changing=0u; // onChange scan priority mask in progress, for rate limiting

    static
    void onChangeComplete(void *usr, IOSCANPVT, int prio) noexcept;

    explicit
    EventQueue(EventLog* log)
        :log(log)
    {
        scanIoInit(&onChange);
        scanIoSetComplete(onChange, onChangeComplete, this);
    }

    static
        EventQueue* getCreate(const std::string& logName,
                              const std::string& queueName) {
        Guard G(eventLogsLock);
        auto& log(eventLogs[logName]);
        if(!log) {
            log.reset(new EventLog(logName));
        }
        auto& queue(log->queues[queueName]);
        if(!queue) {
            queue.reset(new EventQueue(log.get()));
        }
        return queue.get();
    }
};

struct EventDev {
    dbCommon* const prec;
    EventQueue* const queue;
    bool autoclear = false;

    constexpr
    EventDev(dbCommon *prec, EventQueue* queue)
        :prec(prec), queue(queue)
    {}
};

long eventLogInitRecord(dbCommon *prec) noexcept {
    try {
        auto plink(dbGetDevLink(prec));
        assert(plink->type==INST_IO);
        std::string lstr(plink->value.instio.string);


        std::string logName, queueName;
        bool autoclear = true;

        char *saved = nullptr;
        for(char* word = epicsStrtok_r((char*)lstr.data(), " ", &saved)
             ; word
             ; word = epicsStrtok_r(NULL, " ", &saved))
        {
            auto wlen = strlen(word);

            auto cmd = [=](const char *pref) -> const char* {
                auto plen = strlen(pref);
                if(wlen >= plen && memcmp(word, pref, plen)==0) {
                    return word + plen;
                }
                return nullptr;
            };

            if(auto val = cmd("log=")) {
                logName = val;

            } else if(auto val = cmd("queue=")) {
                queueName = val;

            } else if(auto val = cmd("autoclear=")) {
                if(epicsStrCaseCmp(val, "yes")==0) {
                    autoclear = true;
                } else if(epicsStrCaseCmp(val, "no")==0) {
                    autoclear = true;
                } else {
                    throw std::runtime_error("autoclear= must be 'yes' or 'no'");
                }

            } else {
                throw std::runtime_error("Unexpected dev. link parameter");
            }
        }

        if(logName.empty())
            throw std::runtime_error("Missing log=");

        auto log(EventQueue::getCreate(logName, queueName));
        auto pvt = new EventDev(prec, log);
        pvt->autoclear = autoclear;
        prec->dpvt = (void*)pvt;

        return 0;
    } catch(std::exception& e){
        fprintf(stderr, "%s " ERL_ERROR ": %s\n", prec->name, e.what());
        return -1;
    }
}

long eventTableChanged(int detach, struct dbCommon *prec, IOSCANPVT* pscan) noexcept
{
    (void)detach;
    auto pvt = static_cast<EventDev*>(prec->dpvt);
    if(!pvt)
        return -1;

    *pscan = pvt->queue->onChange;
    return 0;
}

#define TRY \
    if(!prec->dpvt) { \
            recGblSetSevrMsg(prec, COMM_ALARM, INVALID_ALARM, "No Init"); \
            return -1; \
    } \
    auto pvt = static_cast<EventDev*>(prec->dpvt); \
    try

#define CATCH \
    catch(std::exception& e){ \
        recGblSetSevrMsg(prec, COMM_ALARM, INVALID_ALARM, "%s", e.what()); \
        if(prec->tpro) \
            errlogPrintf("%s: " ERL_ERROR ": %s\n", prec->name, e.what()); \
        return -1; \
}

long eventLogInput(aaoRecord *prec) noexcept {
    TRY {
        if(prec->ftvl!=menuFtypeULONG) {
            recGblSetSevrMsg(prec, WRITE_ALARM, INVALID_ALARM, "Bad FTVL");
            return -1;
        } else if(prec->nsev>=INVALID_ALARM) {
            // refuse to consume invalid input
            return -1;
        }
        auto log = pvt->queue->log; // queue not relevant

        auto val = static_cast<const epicsUInt32*>(prec->bptr);
        size_t N = prec->nord;


        {
            Guard G(log->lock);

            for(size_t n=0; n+2<N; n+=3) {
                auto evtst = val[n+0];
                auto evt=evtst&0xff;
                if(!evt)
                    continue;

                if(evtst&0x40000000) { // device side overflow before this event
                    log->nOverflows++;
                }

                epicsTimeStamp ts;
                ts.secPastEpoch = val[n+1] - POSIX_TIME_AT_EPICS_EPOCH; // (sec)
                ts.nsec = val[n+2]*log->nsecPerTick + 0.5; // (ns)

                auto it(log->listeners.lower_bound(evt));
                auto end(log->listeners.upper_bound(evt));
                for(; it!=end; ++it) {
                    auto que = it->second;
                    que->last = ts;
                    que->nOccur++;

                    if(que->unused.empty()) {
                        log->nOverflows++;

                    } else {
                        auto it = que->unused.begin();
                        *it = ts;

                        // move first unused to end of queue
                        que->que.splice(que->que.end(),
                                        que->unused,
                                        it);
                    }
                    if(!que->changing)
                        que->changing = scanIoRequest(que->onChange);
                }
            }
        };

        return 0;
    }CATCH
}

void EventQueue::onChangeComplete(void *usr, IOSCANPVT, int prio) noexcept
{
    auto self=static_cast<EventQueue*>(usr);
    try {
        unsigned mask = 1u<<prio;
        Guard G(self->log->lock);
        assert(self->changing & mask);
        self->changing &= ~mask;

    }catch(std::exception& e){
        errlogPrintf("%s: " ERL_ERROR ": %s\n", __func__, e.what());
    }
}

long eventLogSetEvent(longoutRecord *prec) noexcept
{
    if(prec->val<0 || prec->val>255)
        prec->val = 0;

    TRY {

        auto queue = pvt->queue;
        auto log = queue->log;
        Guard G(log->lock);

        if(queue->event) {
            auto it(log->listeners.lower_bound(queue->event));
            auto end(log->listeners.upper_bound(queue->event));
            for(; it!=end; ++it) {
                if(it->second==queue) {
                    log->listeners.erase(it);
                    break;
                }
            }
            queue->event = 0;
        }
        if(prec->val) {
            log->listeners.emplace(prec->val, queue);
            queue->event = prec->val;
        }

        return 0;
    } CATCH
}

long eventLogSetMult(aoRecord *prec) noexcept
{
    if(!isfinite(prec->val) || prec->val<=0.0) {
        recGblSetSevrMsg(prec, WRITE_ALARM, INVALID_ALARM, "Out of range");
        return -1;
    }

    TRY {
        auto log = pvt->queue->log;

        Guard G(log->lock);

        log->nsecPerTick = prec->val;
        // TODO: auto-clear?

        return 0;
    } CATCH
}

long eventLogClear(longoutRecord *prec) noexcept {
    TRY {
        auto queue = pvt->queue;

        {
            Guard G(queue->log->lock);

            if(!prec->val || queue->que.empty())
                return 0;

            // move all queued to unused, append to end
            queue->unused.splice(queue->unused.end(), queue->que);
        }
        scanIoRequest(queue->onChange);

        return 0;
    } CATCH
}

long eventLogOutLast(longinRecord *prec) noexcept
{
    TRY {

        Guard G(pvt->queue->log->lock);

        prec->val = epicsInt32(pvt->queue->nOccur);
        prec->time = pvt->queue->last;

        return 0;
    } CATCH
}

long eventLogInitRecordOutBuf(dbCommon *pcom) noexcept
{
    auto stat = eventLogInitRecord(pcom);
    if(stat)
        return stat;

    auto prec = reinterpret_cast<aaiRecord*>(pcom);
    TRY {
        auto queue = pvt->queue;
        Guard G(queue->log->lock);
        assert(queue->que.empty());

        if(queue->unused.size() < prec->nelm)
            queue->unused.resize(prec->nelm);

        return 0;
    } CATCH
}

long eventLogOutBuf(aaiRecord *prec) noexcept
{
    if(prec->ftvl!=menuFtypeDOUBLE) {
        recGblSetSevrMsg(prec, WRITE_ALARM, INVALID_ALARM, "Bad FTVL");
        return -1;
    }

    auto val = static_cast<double*>(prec->bptr);

    TRY {
        auto queue = pvt->queue;

        Guard G(queue->log->lock);

        if(queue->que.empty()) {
            // leave TIME
            prec->nord = 0;
            return 0;
        }

        auto& t0 = queue->que.front();
        prec->time = t0;

        epicsUInt32 n = 0u;

        auto it(queue->que.begin()),
            end(queue->que.end());

        for(; n<prec->nelm && it!=end; n++, ++it) {
            auto& t = *it;
            val[n] = t-t0;
        }

        prec->nord = n;

        // it points to first element not copied (maybe end)
        if(pvt->autoclear) {
            // move [begin, it) -> unused (append)
            queue->unused.splice(queue->unused.end(),
                                 queue->que,
                                 queue->que.begin(),
                                 it);
        }

        return 0;
    } CATCH
}

aaodset devEventTableInput = {
    {5, nullptr, nullptr, eventLogInitRecord, nullptr},
    eventLogInput,
};
longoutdset devEventTableSetEvent = {
    {5, nullptr, nullptr, eventLogInitRecord, nullptr},
    eventLogSetEvent,
};
aodset devEventTableSetMult = {
    {6, nullptr, nullptr, eventLogInitRecord, nullptr},
    eventLogSetMult, nullptr,
};
longoutdset devEventTableClear = {
    {5, nullptr, nullptr, eventLogInitRecord, nullptr},
    eventLogClear,
};
longindset devEventTableLast = {
    {5, nullptr, nullptr, eventLogInitRecord, eventTableChanged},
    eventLogOutLast,
};
aaidset devEventTableBuf = {
    {5, nullptr, nullptr, eventLogInitRecordOutBuf, eventTableChanged},
    eventLogOutBuf,
};

} // namespace

extern "C" {
epicsExportAddress(dset, devEventTableInput);
epicsExportAddress(dset, devEventTableSetEvent);
epicsExportAddress(dset, devEventTableSetMult);
epicsExportAddress(dset, devEventTableClear);
epicsExportAddress(dset, devEventTableLast);
epicsExportAddress(dset, devEventTableBuf);
}
