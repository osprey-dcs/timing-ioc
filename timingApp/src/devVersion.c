/*************************************************************************\
* Copyright (c) 2026 Osprey Distributed Control Systems
* SPDX-License-Identifier: BSD
\*************************************************************************/

#include <string.h>

#include <stringinRecord.h>
#include <epicsExport.h>

#include "timingVersion.h"

static
long read_version(stringinRecord *prec)
{
    strncpy(prec->val, VCSVERSION, sizeof(prec->val)-1);
    prec->val[sizeof(prec->val)-1] = '\0';
    return 0;
}

static const stringindset devODCSTimingVersion = {
    {5},
    read_version,
};

epicsExportAddress(dset, devODCSTimingVersion);
