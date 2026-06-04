#!/usr/bin/env bash
# Profiling: gprof (perfil llamadas) + /usr/bin/time -v (memoria/SO).
# Nota: perf no esta disponible en WSL2 para este kernel.
set -euo pipefail

cd "$(dirname "$0")/.."
PROF_DIR="results/profile"
rm -rf "$PROF_DIR" && mkdir -p "$PROF_DIR"

N=${N:-5000000}
STEPS=${STEPS:-200}
DENS=${DENS:-0.5}
SEED=${SEED:-7}
THREADS=${THREADS:-8}

echo "[1/4] gprof - serial"
./bin/traffic_serial_prof -n "$N" -s "$STEPS" -d "$DENS" -r "$SEED" -v \
    > "$PROF_DIR/serial_run.txt" 2>&1
mv gmon.out "$PROF_DIR/gmon_serial.out"
gprof ./bin/traffic_serial_prof "$PROF_DIR/gmon_serial.out" \
    > "$PROF_DIR/gprof_serial.txt" 2>/dev/null || true

echo "[2/4] gprof - omp T=1 (gprof no soporta multi-thread)"
OMP_NUM_THREADS=1 ./bin/traffic_omp_prof -n "$N" -s "$STEPS" -d "$DENS" \
    -r "$SEED" -t 1 -v > "$PROF_DIR/omp_t1_run.txt" 2>&1
mv gmon.out "$PROF_DIR/gmon_omp_t1.out"
gprof ./bin/traffic_omp_prof "$PROF_DIR/gmon_omp_t1.out" \
    > "$PROF_DIR/gprof_omp_t1.txt" 2>/dev/null || true

echo "[3/4] /usr/bin/time -v - serial"
/usr/bin/time -v ./bin/traffic_serial -n "$N" -s "$STEPS" -d "$DENS" \
    -r "$SEED" -v > "$PROF_DIR/time_serial.txt" 2>&1 || true

echo "[4/4] /usr/bin/time -v - omp T=$THREADS"
/usr/bin/time -v ./bin/traffic_omp -n "$N" -s "$STEPS" -d "$DENS" \
    -r "$SEED" -t "$THREADS" -S static \
    > "$PROF_DIR/time_omp_t${THREADS}.txt" 2>&1 || true

# Ancho de banda efectivo (bytes movidos por unidad de tiempo)
# Por update: 3 reads + 1 write de int (4B) = 16 B/celda en streaming.
echo "[bw] Calculando ancho de banda efectivo..."
python3 - <<PYEOF > "$PROF_DIR/bandwidth.txt"
import csv, pathlib
def show(path, sched=None):
    rows = list(csv.DictReader(pathlib.Path(path).open()))
    if sched:
        rows = [r for r in rows if r.get('schedule') == sched]
    print(f"== {path}{' ['+sched+']' if sched else ''} ==")
    print(f"{'thr':>4} {'time_s':>9} {'mcups':>10} {'bw_GBs':>10}")
    seen = {}
    for r in rows:
        if r['version'] not in ('serial', 'omp'): continue
        t = int(r['threads']); time = float(r['time_s'])
        mcups = float(r['mcups'])
        bw = mcups * 16 / 1024.0  # 16 B/cell * Mcells/s -> MB/s -> GB/s
        seen.setdefault(t, []).append((time, mcups, bw))
    for t in sorted(seen):
        ts = seen[t]
        avg_t = sum(x[0] for x in ts)/len(ts)
        avg_m = sum(x[1] for x in ts)/len(ts)
        avg_b = sum(x[2] for x in ts)/len(ts)
        print(f"{t:>4} {avg_t:>9.3f} {avg_m:>10.1f} {avg_b:>10.2f}")
    print()

show('results/scaling.csv', 'static')
show('results/scaling.csv', 'guided')
PYEOF

echo "[done] $PROF_DIR"
ls -la "$PROF_DIR"
