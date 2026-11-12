#!/bin/sh
# Run the recompiled original frame on the snapshot; add missing entry points and rebuild until it runs through.
SNAP="${SNAP:-capture/snap2.snap}"
CAR="${CAR:-37e8de20}"
PREV=""
i=0
while [ $i -lt 30 ]; do
  i=$((i + 1))
  OUT=$(timeout 600 ./build-test/recomp_frame.exe re/EBOOT.elf "$SNAP" "$CAR" 2>&1)
  M=$(echo "$OUT" | grep -o "MISSING call target 0x[0-9a-f]*" | grep -o "0x[0-9a-f]*$")
  U=$(echo "$OUT" | grep -o "UNIMPLEMENTED at 0x[0-9a-f]*" | grep -o "0x[0-9a-f]*$")
  if [ -n "$U" ]; then echo "UNIMPLEMENTED $U: $(grep -ho "unimpl($U); return; // .*" src/recomp/full/*.cpp | head -1)"; echo "$OUT" | tail -30; break; fi
  if [ -z "$M" ]; then echo "RAN THROUGH"; echo "$OUT" | tail -40; break; fi
  if [ "$M" = "$PREV" ]; then echo "STUCK on $M"; echo "$OUT" | tail -20; break; fi
  PREV="$M"
  echo "iteration $i: adding entry $M"
  echo "${M#0x}" >> re/extra_entries.txt
  SPLIT=48 /c/Users/User/AppData/Local/Programs/Python/Python312/python tools/ppc2cpp.py src/recomp/full/game.cpp 10000-bb3000 > /dev/null 2>&1
  ( . "/c/Users/User/.local/share/retcomm/toolchains/cmake-clang-v1/latest/env.sh" && cmake --build build-test --target recomp_frame -j 12 2>&1 | grep -E " error" | head -3 )
done
