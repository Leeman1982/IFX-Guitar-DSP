#!/bin/bash
# Boost Factory host test kit.
#   ./validate.sh build                 build the host tools (gcc)
#   ./validate.sh smoke                 run every model: DC/silence + level + speed
#   ./validate.sh spice M S D T V FS A F  compare one circuit stage with ngspice
#        M=model S=stage D=drive(0-1) T=tone(0-1) V=voice FS=stage rate(Hz)
#        A=sine amplitude (V) F=frequency (Hz).  Stage input DC is computed.
#   ./validate.sh alias M F D A         non-harmonic (aliasing) energy, 20 Hz–16 kHz
# Needs: gcc, python3 + numpy, ngspice.
set -e
cd "$(dirname "$0")"
FW=../../firmware/ZombiSS_MultiEffect
SRC="$FW/ifx_boost_factory.c $FW/ifx_bf_models.c $FW/ifx_bf_circuit.c $FW/ifx_bf_math.c"
case "$1" in
  build)
    gcc -O2 -I$FW spval.c  $SRC -lm -o spval
    gcc -O2 -I$FW tsmoke.c $SRC -lm -o tsmoke
    gcc -O2 -I$FW tspec2.c $SRC -lm -o tspec2 ;;
  smoke) ./tsmoke ;;
  spice) shift; echo "$8" > freq.txt; ./spval "$@" -999 >/dev/null
         ngspice -b deck.cir > ng.log 2>&1; python3 cmp.py ;;
  alias) ./tspec2 "$2" "$3" "$4" "$5"; python3 fftal.py "$3"; echo ;;
  *) sed -n 2,11p "$0" ;;
esac
