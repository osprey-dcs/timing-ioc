#!/bin/sh

set -ex
P="EVG:"
if [ $# -gt 0 ]
then
    P="$1"
fi
caput -a "${P}EVG:SEQ:1:pattern" 6 1 2 3 4 1000000000 255
caput -a "${P}EVG:SEQ:2:pattern" 6 10 20 30 40 1000000000 255

caput "${P}EVR:event125:action" 1
caput "${P}EVR:TRIG:1:delay" 0
caput "${P}EVR:TRIG:1:width" 50000000
caput "${P}EVR:TRIG:1:enable" 1

caput "${P}EVR:event004:action" 2
caput "${P}EVR:TRIG:2:delay" 1
caput "${P}EVR:TRIG:2:width" 3
caput "${P}EVR:TRIG:2:enable" 1
