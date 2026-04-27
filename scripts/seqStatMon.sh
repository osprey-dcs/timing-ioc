#!/bin/sh

set -ex
P="EVG:"
if [ $# -gt 0 ]
then
    P="$1"
fi
camonitor -0x "${P}EVG:config" \
              "${P}EVG:status" \
              "${P}EVG:LINK:1:latency" \
              "${P}EVG:LINK:2:latency" \
              "${P}EVG:LINK:3:latency" \
              "${P}EVG:LINK:4:latency" \
              "${P}EVG:LINK:5:latency" \
              "${P}EVG:LINK:6:latency" \
              "${P}EVG:LINK:7:latency" \
              "${P}EVG:LINK:8:latency"
