#!/usr/bin/env bash
set -eo pipefail

# Scripts configs
BIN_DIR="bin"
RESULTS_DIR="results"
CSV_FILE="${RESULTS_DIR}/benchmarks.csv"
RAW_CSV_FILE="${RESULTS_DIR}/benchmarks_raw.csv"

mkdir -p "$RESULTS_DIR"

# Test configuration (Robust)
N_LIST="10000 100000 1000000 10000000"
P_LIST="1 2 4 8"
STEPS=500
DENSITY=0.3
SEED=12345
RUNS=10

# Header for results
echo "N,P,modo,tiempo_s" > "$CSV_FILE"
echo "N,P,modo,run,tiempo_s" > "$RAW_CSV_FILE"

echo "=== MPI Traffic Benchmark ==="
echo "NSTEPS: ${STEPS}, DENSITY: ${DENSITY}, RUNS: ${RUNS}"

function run_and_avg() {
    local cmd="$1"
    local n_val="$2"
    local p_val="$3"
    local modo_val="$4"
    local total=0
    for ((i=1; i<=RUNS; i++)); do
        local out
        out=$(eval "$cmd")
        local time_val
        time_val=$(echo "$out" | awk '{print $6}')
        # Registrar dato de corrida individual en Raw CSV
        echo "$n_val,$p_val,$modo_val,$i,$time_val" >> "$RAW_CSV_FILE"
        total=$(echo "$total + $time_val" | bc -l)
    done
    echo "$total / $RUNS" | bc -l | awk '{printf "%.5f", $0}'
}

for N in $N_LIST; do
    echo "--- Evaluando N = $N ---"
    
    # 1. Ejecutar serial
    CMD_SERIAL="${BIN_DIR}/traffic_serial -n $N -s $STEPS -d $DENSITY -r $SEED"
    SERIAL_TIME=$(run_and_avg "$CMD_SERIAL" "$N" "1" "serial")
    echo "$N,1,serial,$SERIAL_TIME" >> "$CSV_FILE"
    echo "Serial (P=1): $SERIAL_TIME s"

    for P in $P_LIST; do
        if [ "$P" -eq 1 ]; then continue; fi

        # 2. Ejecutar MPI ASYNC
        export SYNC_MODE=""
        CMD_ASYNC="mpirun --oversubscribe -np $P ${BIN_DIR}/traffic_mpi -n $N -s $STEPS -d $DENSITY -r $SEED"
        ASYNC_TIME=$(run_and_avg "$CMD_ASYNC" "$N" "$P" "async")
        echo "$N,$P,async,$ASYNC_TIME" >> "$CSV_FILE"
        echo "MPI Async (P=$P): $ASYNC_TIME s"

        # 3. Ejecutar MPI SYNC
        export SYNC_MODE="1"
        CMD_SYNC="mpirun --oversubscribe -np $P ${BIN_DIR}/traffic_mpi -n $N -s $STEPS -d $DENSITY -r $SEED"
        SYNC_TIME=$(run_and_avg "$CMD_SYNC" "$N" "$P" "sync")
        echo "$N,$P,sync,$SYNC_TIME" >> "$CSV_FILE"
        echo "MPI Sync (P=$P): $SYNC_TIME s"
    done
    echo ""
done

# Print Speedup and Efficiency Table
echo ""
echo "=== Análisis de Rendimiento ==="
printf "%-10s %-5s %-10s %-17s %-15s %-15s\n" "N" "P" "Modo" "Tiempo (Promedio)" "Speedup" "Eficiencia"
echo "-----------------------------------------------------------------------------------"

for N in $N_LIST; do
    T_SERIAL=$(awk -F, -v n=$N '$1==n && $3=="serial" {print $4}' "$CSV_FILE")
    for P in $P_LIST; do
        if [ "$P" -eq 1 ]; then
            printf "%-10s %-5s %-10s %-17s %-15.4f %-15.4f\n" "$N" "$P" "serial" "$T_SERIAL" "1.0" "1.0"
            continue
        fi

        for MODE in "async" "sync"; do
            T_MPI=$(awk -F, -v n=$N -v p=$P -v m=$MODE '$1==n && $2==p && $3==m {print $4}' "$CSV_FILE")
            if [ -n "$T_MPI" ] && [ -n "$T_SERIAL" ]; then
                SPEEDUP=$(echo "scale=4; $T_SERIAL / $T_MPI" | bc -l)
                EFFICIENCY=$(echo "scale=4; $SPEEDUP / $P" | bc -l)
                printf "%-10s %-5s %-10s %-17s %-15s %-15s\n" "$N" "$P" "$MODE" "$T_MPI" "$SPEEDUP" "$EFFICIENCY"
            fi
        done
    done
    echo "-----------------------------------------------------------------------------------"
done
