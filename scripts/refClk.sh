#!/bin/sh

P="EVG:"
C="SI570"

if [ $# -gt 0 ]
then
    case "$1" in
    [Rr])   C="FPGA_REFCLK0" ;;
    [Ff])   C="FMC1_GBTCLK0" ;;
    [Ss])   C="SI570" ;;
    esac
fi
if [ $# -gt 1 ]
then
    P="$2"
fi

set -ex
caput "${P}MARBLE:MGTCLK0" "${C}"

