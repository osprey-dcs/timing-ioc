#!/bin/sh

# Send a software event request

set -e
P="EVG:"
EV=1
for i in "$@"
do
    case "$i" in
    [0-9][0-9]*) EV="$i" ;;
    *)           P="$i"  ;;
    esac
done

set -x
caput "${P}EVG:swEvent"  "$EV"
