# SPDX-FileCopyrightText: 2003 Argonne National Laboratory
#
# SPDX-License-Identifier: EPICS

TOP=..

include $(TOP)/configure/CONFIG
#----------------------------------------
#  ADD MACRO DEFINITIONS AFTER THIS LINE
#=============================

#=============================
# Build the IOC application

PROD_IOC = ospreyTimingIoc
# ospreyTimingIoc.dbd will be created and installed
DBD += ospreyTimingIoc.dbd

# ospreyTimingIoc.dbd will be made up from these files:
ospreyTimingIoc_DBD += base.dbd
ospreyTimingIoc_DBD += system.dbd
ospreyTimingIoc_DBD += feed.dbd
ospreyTimingIoc_DBD += asSupport.dbd
ospreyTimingIoc_DBD += ospreyTiming.dbd

# Add all the support libraries needed by this IOC
ospreyTimingIoc_LIBS += ospreyTiming
ospreyTimingIoc_LIBS += feed
ospreyTimingIoc_LIBS += autosave

# ospreyTimingIoc_registerRecordDeviceDriver.cpp derives from ospreyTimingIoc.dbd
ospreyTimingIoc_SRCS += ospreyTimingIoc_registerRecordDeviceDriver.cpp

# Build the main IOC entry point on workstation OSs.
ospreyTimingIoc_SRCS_DEFAULT += iocMain.cpp
ospreyTimingIoc_SRCS_vxWorks += -nil-

ifneq (,$(CAPUTLOG))
ospreyTimingIoc_DBD += caPutLog.dbd
ospreyTimingIoc_LIBS += caPutLog
endif

ifneq (,$(RECSYNC))
ospreyTimingIoc_DBD += reccaster.dbd
ospreyTimingIoc_LIBS += reccaster
endif

ifdef LINSTAT_0_0_0
ospreyTimingIoc_DBD += linStat.dbd
ospreyTimingIoc_LIBS += linStat
endif

ifdef PVXS_MAJOR_VERSION # prefer v2
ospreyTimingIoc_DBD += pvxsIoc.dbd
ospreyTimingIoc_LIBS += pvxsIoc pvxs
endif

# Finally link to the EPICS Base libraries
ospreyTimingIoc_LIBS += $(EPICS_BASE_IOC_LIBS)

#===========================

include $(TOP)/configure/RULES
#----------------------------------------
#  ADD RULES AFTER THIS LINE

