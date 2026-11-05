#!/bin/bash
# Host tests for the ZombiSS effect chain (delay, reverb, switching).
# Needs only gcc.  Usage: ./run.sh
set -e
cd "$(dirname "$0")"
FW=../../firmware/ZombiSS_MultiEffect
gcc -O2 -Wall -Wno-unused-function -I"$FW" test_chain.c "$FW"/*.c -lm -o test_chain
./test_chain
