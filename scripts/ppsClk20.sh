#!/bin/sh

P="EVG:"
if [ $# -gt 0 ]
then
    P="$1"
fi

set -ex
caput "${P}MARBLE:VCXO:DAC_Y1" 0
caput "${P}MARBLE:VCXO:DAC_Y3" 13500
caput "${P}MARBLE:PPS:Local" 2
caput "${P}MARBLE:VCXO:DAC_Y1" -32768
