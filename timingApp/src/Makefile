# SPDX-FileCopyrightText: 2003 Argonne National Laboratory
#
# SPDX-License-Identifier: EPICS

TOP=../..

include $(TOP)/configure/CONFIG
#----------------------------------------
#  ADD MACRO DEFINITIONS AFTER THIS LINE
#=============================

#=============================
# Build the IOC application

LIBRARY_IOC = ospreyTiming
# evg.dbd will be created and installed
DBD += ospreyTiming.dbd

ospreyTiming_SRCS += copyTime.c
ospreyTiming_SRCS += goldenBoot.c
ospreyTiming_SRCS += bitTable.cpp
ospreyTiming_SRCS += eventTable.cpp
ospreyTiming_SRCS += seqMux.c

# Finally link to the EPICS Base libraries
ospreyTiming_LIBS += $(EPICS_BASE_IOC_LIBS)

#===========================

include $(TOP)/configure/RULES
#----------------------------------------
#  ADD RULES AFTER THIS LINE

