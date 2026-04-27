#!/bin/sh

set -ex
P="EVG:"
if [ $# -gt 0 ]
then
    P="$1"
fi
caput "${P}EVG:SEQ:arm" 3
sleep 5
caput "${P}EVG:SEQ:swTrig" 3
