#!/bin/sh
# prints time to reach 20..170 km/h for the port (physlab, flat ground) next to the original measurement
./build-test/physlab.exe ../NPEB00060/USRDIR/data/cars/pack_original_0/minicooper_0/minicooper.ep3 accel 2>/dev/null | awk '
BEGIN { n=split("20 40 60 80 100 120 140 160 170", M, " "); split("0.32 0.65 1.15 1.74 2.37 3.66 4.94 7.31 8.70", O, " "); k=1 }
/^t=/ { t=$2+0; sub("t=","",$1); t=$1+0; v=$2; sub("v=","",v); sub("kmh","",v); v=v+0;
        while (k<=n && v>=M[k]) { printf "%4d km/h  port %5.2f s   original %5.2f s\n", M[k], t, O[k]; k++ } }
END { while (k<=n) { printf "%4d km/h  port   --     original %5.2f s\n", M[k], O[k]; k++ } }'
