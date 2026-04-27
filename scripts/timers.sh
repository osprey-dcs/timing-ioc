#!/bin/sh

set -ex

P="EVG:"
if [ $# -gt 0 ]
then
    P="$1"
fi

caput "${P}EVG:TMR:1:event" 41
caput "${P}EVG:TMR:1:divisor" 100
caput "${P}EVG:TMR:2:event" 42
caput "${P}EVG:TMR:2:divisor" 101

caput "${P}EVG:TMR:start" 1
caget -0x "${P}EVG:TMR:status"

