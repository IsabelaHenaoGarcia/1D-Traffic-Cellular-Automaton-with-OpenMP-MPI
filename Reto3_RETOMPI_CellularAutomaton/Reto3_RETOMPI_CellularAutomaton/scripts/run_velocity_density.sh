#!/usr/bin/env bash
# Barrido de densidad para reproducir la curva velocidad-densidad clasica.
set -euo pipefail

cd "$(dirname "$0")/.."
OUT="results/velocity_density.csv"
rm -f "$OUT"

N=${N:-2000000}
STEPS=${STEPS:-400}
SEED=${SEED:-101}
THREADS=${THREADS:-8}
BIN=${BIN:-./bin/traffic_omp}

echo "[exp] velocity-density sweep: N=$N steps=$STEPS threads=$THREADS"

for d in 0.05 0.10 0.15 0.20 0.25 0.30 0.35 0.40 0.45 0.50 0.55 0.60 \
         0.65 0.70 0.75 0.80 0.85 0.90 0.95; do
    "$BIN" -n "$N" -s "$STEPS" -d "$d" -r "$SEED" -t "$THREADS" \
        -o "$OUT" --tag vd -v 2>&1 | tail -1
done

echo "[done] $OUT"
