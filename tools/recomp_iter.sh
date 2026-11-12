#!/bin/sh
# Grow the translated ranges until the car update runs on the snapshot without missing calls.
# usage: RANGES="lo-hi ..." SNAP=capture/snap2.snap CAR=37e8de20 sh tools/recomp_iter.sh
RANGES="${RANGES:-300000-350000 260000-290000 99c000-9b0000 910000-920000 9c0000-9d0000 960000-970000}"
SNAP="${SNAP:-capture/snap2.snap}"
CAR="${CAR:-37e8de20}"
PREV=""
i=0
while [ $i -lt 40 ]; do
  i=$((i + 1))
  /c/Users/User/AppData/Local/Programs/Python/Python312/python tools/ppc2cpp.py src/recomp/gen/physics.cpp $RANGES > capture/tr.txt 2>&1
  ( . "/c/Users/User/.local/share/retcomm/toolchains/cmake-clang-v1/latest/env.sh" && cmake --build build-test --target recomp_test 2>&1 | grep -E " error" | head -3 )
  OUT=$(./build-test/recomp_test.exe re/EBOOT.elf step "$SNAP" "$CAR" "$SNAP.next")
  M=$(echo "$OUT" | grep -o "MISSING call target 0x[0-9a-f]*" | grep -o "0x[0-9a-f]*$")
  U=$(echo "$OUT" | grep -o "UNIMPLEMENTED instruction at 0x[0-9a-f]*")
  if [ -n "$U" ]; then echo "$U"; echo "$OUT"; echo "RANGES=$RANGES"; break; fi
  if [ -z "$M" ]; then echo "DONE"; echo "$OUT"; echo "RANGES=$RANGES"; break; fi
  if [ "$M" = "$PREV" ]; then
    if [ "$M" = "$PREV2" ]; then echo "STUCK on $M"; echo "$OUT"; echo "RANGES=$RANGES"; break; fi
    echo "${M#0x}" >> re/extra_entries.txt; echo "iteration $i: $M added as entry point"; PREV2="$M"; continue
  fi
  PREV="$M"
  BASE=$(( M / 65536 * 65536 ))
  LO=$(printf "%x" $BASE)
  HI=$(printf "%x" $(( BASE + 65536 )))
  echo "iteration $i: missing $M -> adding $LO-$HI"
  RANGES="$RANGES $LO-$HI"
done
