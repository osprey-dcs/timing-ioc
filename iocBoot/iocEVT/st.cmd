#!../../bin/linux-x86_64/ospreyTimingIoc

epicsEnvSet(EVT_IPADDR, "$(EVT_IPADDR=127.0.0.1)")
epicsEnvSet("P", "EVT:")

## Register all support components
dbLoadDatabase "../../dbd/ospreyTimingIoc.dbd"
ospreyTimingIoc_registerRecordDeviceDriver(pdbbase)

dbLoadRecords("../../db/ospreyEVT.db","P=$(P),NAME=EVT,IPADDR=$(EVT_IPADDR)")

system "install -d as"

set_savefile_path("$(PWD)", "/as")
set_requestfile_path("$(PWD)", "/as")

set_pass0_restoreFile("evt_settings.sav")
set_pass1_restoreFile("evt_waveforms.sav")

iocInit()

makeAutosaveFileFromDbInfo("$(PWD)/as/evt_settings.req", "autosaveFields_pass0")
makeAutosaveFileFromDbInfo("$(PWD)/as/evt_waveforms.req", "autosaveFields_pass1")

create_monitor_set("evt_settings.req", 10 , "")
create_monitor_set("evt_waveforms.req", 30 , "")
