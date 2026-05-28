set -ex

P="EVG:"
if [ $# -gt 0 ]
then
    P="$1"
fi

set -ex
caput "${P}Marble:VCXO:DAC_Y1" 0
caput "${P}Marble:PPS:Local" 1
