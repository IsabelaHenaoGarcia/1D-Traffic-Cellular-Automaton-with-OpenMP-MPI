#!/usr/bin/env bash
# Demostracion del impacto de anti-patrones (critical, atomic, false sharing).
set -euo pipefail

cd "$(dirname "$0")/.."
OUT="results/antipatterns.csv"
rm -f "$OUT"

N=${N:-5000000}
STEPS=${STEPS:-50}
DENS=${DENS:-0.5}
SEED=${SEED:-7}
THREADS=${THREADS:-8}
REPS=${REPS:-3}

for r in $(seq 1 "$REPS"); do
    ./bin/traffic_serial -n "$N" -s "$STEPS" -d "$DENS" -r "$SEED" \
        -o "$OUT" --tag "ref_serial_r$r" -v 2>&1 | tail -1
    ./bin/traffic_omp -n "$N" -s "$STEPS" -d "$DENS" -r "$SEED" \
        -t "$THREADS" -S static -o "$OUT" --tag "ref_omp_r$r" -v 2>&1 | tail -1
    ./bin/traffic_omp_bad -n "$N" -s "$STEPS" -d "$DENS" -r "$SEED" \
        -t "$THREADS" --bad critical -o "$OUT" --tag "bad_critical_r$r" 2>&1 \
        | tail -1
    ./bin/traffic_omp_bad -n "$N" -s "$STEPS" -d "$DENS" -r "$SEED" \
        -t "$THREADS" --bad atomic -o "$OUT" --tag "bad_atomic_r$r" 2>&1 \
        | tail -1
    ./bin/traffic_omp_bad -n "$N" -s "$STEPS" -d "$DENS" -r "$SEED" \
        -t "$THREADS" --bad falseshare -o "$OUT" --tag "bad_falseshare_r$r" \
        2>&1 | tail -1
done

echo "[done] $OUT"
