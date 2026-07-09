#!/bin/sh

P="EVG:"
if [ $# -gt 0 ]
then
    P="$1"
fi

set -ex
caput "${P}FPGA:activateEVG" 1
