#!/usr/bin/env bash
# Efecto del chunk size en static y dynamic.
# Nota: dynamic chunk=1 es 50x mas lento que static (overhead de sync).
set -euo pipefail

cd "$(dirname "$0")/.."
OUT="results/chunk_sweep.csv"
rm -f "$OUT"

N=${N:-20000000}
STEPS=${STEPS:-100}
DENS=${DENS:-0.5}
SEED=${SEED:-7}
THREADS=${THREADS:-8}
REPS=${REPS:-2}

STATIC_CHUNKS=${STATIC_CHUNKS:-"1 16 256 4096 65536 524288"}
DYNAMIC_CHUNKS=${DYNAMIC_CHUNKS:-"4096 16384 65536 262144 1048576"}

for c in $STATIC_CHUNKS; do
    for r in $(seq 1 "$REPS"); do
        ./bin/traffic_omp -n "$N" -s "$STEPS" -d "$DENS" -r "$SEED" \
            -t "$THREADS" -S static -c "$c" \
            -o "$OUT" --tag "chunk_static_${c}_r$r" -v 2>&1 | tail -1
    done
done

for c in $DYNAMIC_CHUNKS; do
    for r in $(seq 1 "$REPS"); do
        ./bin/traffic_omp -n "$N" -s "$STEPS" -d "$DENS" -r "$SEED" \
            -t "$THREADS" -S dynamic -c "$c" \
            -o "$OUT" --tag "chunk_dynamic_${c}_r$r" -v 2>&1 | tail -1
    done
done

echo "[done] $OUT"
