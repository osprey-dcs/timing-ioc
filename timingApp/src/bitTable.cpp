/*************************************************************************\
* Copyright (c) 2026 Osprey Distributed Control Systems
* SPDX-License-Identifier: BSD
\*************************************************************************/
/* Bit table manager
 *
 * Maintain a table of EVR action bit masks.
 * Each row is 1 or more 32-bit words holding bit masks.
 * Expected to be a sparse mapping.
 */

#include <map>
#include <string>
#include <memory>
#include <stdexcept>

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
#include <aaiRecord.h>
#include <longoutRecord.h>
#include <menuFtype.h>

#include <epicsExport.h>

namespace {

typedef epicsGuard<epicsMutex> Guard;

struct BitTable;
struct BitDev;

epicsMutex bitTablesLock;
std::map<std::string, std::unique_ptr<BitTable>> bitTables;

struct BitTable {
    const std::string name;
    const unsigned nEvents = 256u;
    IOSCANPVT onChange;

    epicsMutex lock;

    unsigned bitsPerEvent=0u;  // row size in bits
    unsigned wordsPerEvent=0u; // # of 32-bit words used to store bits

    // event, action, active (always true)
    std::map<uint8_t, std::map<uint32_t, bool>> table;

    bool changing = false;

    explicit
    BitTable(const std::string& name)
        :name(name)
    {
        scanIoInit(&onChange);
    }

    static
    BitTable* getCreate(const std::string& name) {
        Guard G(bitTablesLock);
        auto it(bitTables.find(name));
        if(it!=bitTables.end())
            return it->second.get();

        std::unique_ptr<BitTable> tbl(new BitTable(name));
        auto pair(bitTables.emplace(name, std::move(tbl)));
        assert(pair.second);
        return pair.first->second.get();
    }
};

long bitTableReport(int lvl) noexcept
{
    try {
        Guard T(bitTablesLock);

        for(auto& pair : bitTables) {
            auto& tbl = *pair.second;
            Guard G(tbl.lock);

            printf("  \"%s\" : width: %u bits / %u words\n",
                   pair.first.c_str(), tbl.bitsPerEvent, tbl.wordsPerEvent);

            if(lvl<=0)
                continue;

            printf("    EVT# = action bit indicies\n");

            for(auto epair : tbl.table) {
                printf("    % 3d -", epair.first);

                for(auto apair : epair.second) {
                    if(apair.second)
                        printf(" %u", apair.first);
                }
                printf("\n");
            }
        }

        return 0;
    } catch(std::exception& e) {
        fprintf(stderr, "%s " ERL_ERROR ": %s\n", __func__, e.what());
        return -1;
    }
}

drvet drvBitTable = {
    2, bitTableReport, NULL,
};

struct BitDev {
    dbCommon* const prec;
    BitTable* const table;
    const int action;

    uint8_t prevEvent = 0; // must lock BitTable::lock for read and write

    BitDev(dbCommon *prec, BitTable* table, int action)
        :prec(prec), table(table), action(action)
    {}
};

long bitTableInitRecord(dbCommon *prec) noexcept {
    try {
        auto plink(dbGetDevLink(prec));
        assert(plink->type==INST_IO);
        std::string lstr(plink->value.instio.string);


        std::string tableName;
        int action = -1;

        char *saved = nullptr;
        for(char* word = epicsStrtok_r(lstr.data(), " ", &saved)
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

            if(auto val = cmd("table=")) {
                tableName = val;

            } else if(auto val = cmd("action=")) {
                action = std::stoi(val, nullptr, 0);

            } else {
                throw std::runtime_error("Unexpected dev. link parameter");
            }
        }

        if(tableName.empty())
            throw std::runtime_error("Missing table=");

        auto table(BitTable::getCreate(tableName));
        auto pvt = new BitDev(prec, table, action);
        prec->dpvt = (void*)pvt;

        return 0;
    } catch(std::exception& e){
        fprintf(stderr, "%s " ERL_ERROR ": %s\n", prec->name, e.what());
        return -1;
    }
}

#define TRY \
    if(!prec->dpvt) { \
        recGblSetSevrMsg(prec, COMM_ALARM, INVALID_ALARM, "No Init"); \
        return -1; \
    } \
    auto pvt = static_cast<BitDev*>(prec->dpvt); \
    try

#define CATCH \
    catch(std::exception& e){ \
    recGblSetSevrMsg(prec, COMM_ALARM, INVALID_ALARM, "%s", e.what()); \
    if(prec->tpro) \
        errlogPrintf("%s: " ERL_ERROR ": %s\n", prec->name, e.what()); \
    return -1; \
    }

long bitTableSetWords(longoutRecord *prec) noexcept
{
    TRY {
        if(prec->val<=0) {
            recGblSetSevrMsg(prec, WRITE_ALARM, INVALID_ALARM, "Range");
            return -1;
        }

        unsigned nbit = prec->val;
        // round up to multiple of 32
        nbit--;
        nbit |= 0x1f;
        nbit++;
        unsigned nwords = nbit/32u;

        {
            Guard G(pvt->table->lock);

            pvt->table->wordsPerEvent = nwords;
            pvt->table->bitsPerEvent = prec->val; // store original

            pvt->table->changing = true;
        }
        scanIoRequest(pvt->table->onChange);

        return 0;
    } CATCH
}

longoutdset devBitTableSetWords = {
    {5, NULL, NULL, bitTableInitRecord, NULL,
    },
    bitTableSetWords,
};

long bitTableUpdate(longoutRecord *prec) noexcept
{
    TRY {
        if(prec->val < 0 || prec->val > 255) {
            prec->val = 0;
        }
        uint8_t newEvent = prec->val;

        if(pvt->action<0) {
            recGblSetSevrMsg(prec, WRITE_ALARM, INVALID_ALARM, "No Action");
            return -1;
        }

        bool change;
        {
            Guard G(pvt->table->lock);

            if(newEvent==pvt->prevEvent)
                return 0; // no-op

            // clear previous
            if(pvt->prevEvent) {
                auto eit = pvt->table->table.find(pvt->prevEvent);
                if(eit!=pvt->table->table.end()) {
                    auto& row = eit->second;
                    bool erased = row.erase(pvt->action);
                    assert(erased);
                    if(eit->second.empty()) {
                        pvt->table->table.erase(eit); // remove empty row
                    }
                }
                pvt->prevEvent = 0;
            }
            // set new
            if(newEvent) {
                auto& cur = pvt->table->table[newEvent][pvt->action]; // implicitly alloc with false
                if(cur) {
                    recGblSetSevrMsg(prec, WRITE_ALARM, INVALID_ALARM, "Duplicate");
                    return -1;
                }
                cur = true;
            }
            pvt->prevEvent = newEvent;

            change = !pvt->table->changing;
            pvt->table->changing = true;
        }
        if(change)
            scanIoRequest(pvt->table->onChange);

        return 0;
    } CATCH
}

longoutdset devBitTableUpdate = {
    {5, NULL, NULL, bitTableInitRecord, NULL,
    },
    bitTableUpdate,
};

long bitTableChanged(int detach, struct dbCommon *prec, IOSCANPVT* pscan) noexcept
{
    (void)detach;
    auto pvt = static_cast<BitDev*>(prec->dpvt);
    if(!pvt)
        return -1;

    *pscan = pvt->table->onChange;
    return 0;
}

long bitTableRead(aaiRecord *prec) noexcept
{
    if(prec->ftvl != menuFtypeULONG) {
        recGblSetSevrMsg(prec, READ_ALARM, INVALID_ALARM, "Bad FTVL");
        return -1;
    }

    TRY {
        Guard G(pvt->table->lock);
        pvt->table->changing = false;

        auto wordsPerEvent = pvt->table->wordsPerEvent;
        auto bitsPerEvent = pvt->table->bitsPerEvent;
        epicsUInt32 cap = pvt->table->nEvents * wordsPerEvent;

        if(prec->nelm < cap) {
            recGblSetSevrMsg(prec, READ_ALARM, INVALID_ALARM, "Bad NELM");
            return -1;
        }
        auto val = static_cast<epicsUInt32*>(prec->bptr);
        memset(val, 0, cap*4u);

        prec->nord = 0; // in case something exceptional happens

        for(auto epair : pvt->table->table) {

            for(auto apair : epair.second) {
                if(!apair.second) {
                    continue;

                } else if(apair.first >= bitsPerEvent) {
                    recGblSetSevrMsg(prec, READ_ALARM, MAJOR_ALARM, "OoR %u", unsigned(apair.first));
                    continue;
                }

                auto idx = apair.first / 32u;
                auto bit = apair.first % 32u;
                auto mask = 1u << bit;

                idx = wordsPerEvent - 1u - idx; // high word first
                idx += epair.first*wordsPerEvent;

                val[idx] |= mask;
            }
        }

        prec->nord = cap;

        return 0;
    } CATCH
}

aaidset devBitTableRead = {
    {5, NULL, NULL, bitTableInitRecord, bitTableChanged},
    bitTableRead,
};

} // namespace

extern "C" {
epicsExportAddress(dset, devBitTableSetWords);
epicsExportAddress(dset, devBitTableUpdate);
epicsExportAddress(dset, devBitTableRead);
epicsExportAddress(drvet, drvBitTable);
}
