#!/usr/bin/env bash
# Strong scaling: tiempo vs numero de hilos para N fijo, distintos schedulers.
set -euo pipefail

cd "$(dirname "$0")/.."
OUT="results/scaling.csv"
rm -f "$OUT"

N=${N:-20000000}
STEPS=${STEPS:-100}
DENS=${DENS:-0.5}
SEED=${SEED:-7}
REPS=${REPS:-3}

THREADS_LIST=${THREADS_LIST:-"1 2 3 4 6 8 10 12"}
SCHEDULERS=${SCHEDULERS:-"static dynamic guided"}

echo "[exp] strong scaling: N=$N steps=$STEPS dens=$DENS reps=$REPS"

# Baseline serial (reps)
for r in $(seq 1 "$REPS"); do
    ./bin/traffic_serial -n "$N" -s "$STEPS" -d "$DENS" -r "$SEED" \
        -o "$OUT" --tag "serial_r$r" -v 2>&1 | tail -1
done

# OMP runs
for sch in $SCHEDULERS; do
    for t in $THREADS_LIST; do
        for r in $(seq 1 "$REPS"); do
            ./bin/traffic_omp -n "$N" -s "$STEPS" -d "$DENS" -r "$SEED" \
                -t "$t" -S "$sch" -o "$OUT" --tag "omp_${sch}_r$r" -v 2>&1 \
                | tail -1
        done
    done
done

echo "[done] $OUT"
