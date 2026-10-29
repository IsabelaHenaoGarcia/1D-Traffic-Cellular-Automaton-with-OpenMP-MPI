# Reto 03: Cellular Automaton Traffic Simulation (Nagel-Schreckenberg Model)

This repository contains the complete implementation, performance profiling, and comparative scaling analysis for a 1D Traffic Flow Cellular Automaton based on the **Nagel-Schreckenberg (NaSch) Model**. The project explores parallelization techniques using **OpenMP** (shared memory) and **MPI** (distributed memory), evaluating performance metrics such as execution time, speedup, efficiency, throughput (updates per second), and antipattern bottlenecks across varying system sizes and thread/process counts.

---

## Directory & File Structure

```text
Reto3-Reto-MPI-main/
└── Reto3_RETOMPI_CellularAutomaton/
    ├── INFORME.md                        # Comprehensive technical report
    ├── Makefile                          # Build automation system
    ├── README.md                         # Project documentation overview
    ├── trafficmodel.pdf                  # Theoretical model reference
    ├── bin/                              # Compiled executable binaries
    │   ├── traffic_serial                # Base sequential executable
    │   ├── traffic_serial_prof           # Profiling-enabled serial binary (-pg)
    │   ├── traffic_omp                   # Optimized OpenMP parallel executable
    │   ├── traffic_omp_bad               # OpenMP executable exhibiting antipatterns
    │   ├── traffic_omp_prof              # OpenMP binary compiled with profiling
    │   └── traffic_mpi                   # MPI distributed-memory executable
    ├── src/                              # C source code files
    │   ├── common.h                      # Shared structures, parameters, and timer utilities
    │   ├── traffic_serial.c              # Sequential NaSch model implementation
    │   ├── traffic_omp.c                 # Optimized OpenMP implementation
    │   ├── traffic_omp_bad.c             # OpenMP implementation with deliberate antipatterns
    │   └── traffic_mpi.c                 # MPI domain-decomposition implementation
    ├── scripts/                          # Experiment automation & evaluation scripts
    │   ├── run_benchmarks.sh             # Benchmark suite across problem sizes
    │   ├── run_scaling.sh                # Strong and weak scaling evaluation
    │   ├── run_chunk_sweep.sh            # OMP schedule chunk-size sweep script
    │   ├── run_antipatterns.sh           # Antipattern vs optimized performance comparison
    │   ├── run_profiling.sh              # CPU profiling and hardware counter trace script
    │   ├── run_velocity_density.sh       # Traffic fundamental diagram simulation
    │   └── plot_results.py               # Python visualization script (Matplotlib/Pandas)
    ├── results/                          # Raw CSV logs and gprof performance trace files
    │   ├── benchmarks.csv                # Processed benchmark metrics
    │   ├── benchmarks_raw.csv            # Raw timing output logs
    │   ├── scaling.csv                   # Speedup and efficiency datasets
    │   ├── chunk_sweep.csv               # Schedule/chunk evaluation results
    │   ├── antipatterns.csv              # Antipattern impact raw data
    │   ├── velocity_density.csv          # Density vs velocity metrics
    │   └── profile/                      # Profiling outputs (gmon.out, gprof summaries)
    └── plots/                            # High-resolution generated performance graphs (.png)
        ├── scaling.png                   # Speedup vs number of worker threads/processes
        ├── efficiency.png                # Parallel efficiency curves
        ├── chunk_sweep.png               # Execution time across schedule chunk sizes
        ├── antipatterns.png              # False sharing & synchronization overhead comparison
        └── velocity_vs_density.png       # Fundamental traffic flow diagram
```

---

## Technical Overview & Model Definition

The **Nagel-Schreckenberg (NaSch) model** is a discrete 1D cellular automaton that models highway traffic dynamics. A road of length $L$ is divided into discrete cells occupied by cars with velocity $v_i \in [0, v_{\max}]$.

In each time step, all vehicles update their states synchronously according to four rules:

1. **Acceleration:** $v_i \leftarrow \min(v_i + 1, v_{\max})$
2. **Deceleration (Collision Avoidance):** $v_i \leftarrow \min(v_i, d_i - 1)$, where $d_i$ is the distance to the vehicle ahead.
3. **Randomization (Driver Fluctuation):** With probability $p$, if $v_i > 0$, then $v_i \leftarrow v_i - 1$.
4. **Vehicle Movement:** Position $x_i \leftarrow (x_i + v_i) \pmod L$.

### Parallelization Strategies

* **OpenMP (Shared Memory):**
  * Loop-level parallelization of cell updating routines using `#pragma omp parallel for`.
  * Elimination of false sharing by enforcing proper alignment and private iteration space.
  * Schedule optimization (`static`, `dynamic`, `guided`) with custom chunk size tuning.

* **MPI (Distributed Memory):**
  * 1D domain decomposition where the global road array is partitioned into contiguous sub-domains among $P$ MPI ranks.
  * Halo cell / Ghost cell exchange ($v_{\max}$ boundary cells) using point-to-point communication (`MPI_Sendrecv` or non-blocking primitives).
  * Periodic boundary conditions maintained globally across rank $0$ and rank $P-1$.

---

## Build Instructions

A unified `Makefile` is provided to build all implementations:

```bash
cd Reto3_RETOMPI_CellularAutomaton/Reto3_RETOMPI_CellularAutomaton

# Build all binaries
make all

# Build individual targets
make serial       # Compiles bin/traffic_serial
make omp          # Compiles bin/traffic_omp
make omp_bad      # Compiles bin/traffic_omp_bad
make mpi          # Compiles bin/traffic_mpi
make prof         # Compiles profiling binaries with -pg flag

# Clean generated binaries and build artifacts
make clean
```

---

## Running Experiments & Scripts

Automated Bash scripts in the `scripts/` directory handle execution, profiling, and benchmarking.

### 1. Scaling Benchmark
Executes strong and weak scaling sweeps across varying thread and rank counts:
```bash
./scripts/run_scaling.sh
```

### 2. Antipattern Impact Analysis
Compares optimized OpenMP implementations against unoptimized variants (evaluating false sharing and excessive lock contention):
```bash
./scripts/run_antipatterns.sh
```

### 3. Chunk Size Sweep
Evaluates loop scheduling strategies (`static`, `dynamic`, `guided`) across different chunk sizes:
```bash
./scripts/run_chunk_sweep.sh
```

### 4. CPU Profiling
Generates `gprof` call graphs and execution bottleneck reports:
```bash
./scripts/run_profiling.sh
```

### 5. Fundamental Traffic Diagram
Simulates velocity vs. density relationships across different vehicle occupancy rates:
```bash
./scripts/run_velocity_density.sh
```

---

## Visualization & Data Processing

To process CSV results and regenerate figures in the `plots/` directory:

```bash
python3 scripts/plot_results.py
```

### Key Metrics Tracked
* **Speedup ($S_p$):** $S_p = \frac{T_1}{T_p}$
* **Efficiency ($E_p$):** $E_p = \frac{S_p}{p}$
* **Throughput:** Millions of cell updates per second (MCUPS).
* **Traffic Flow ($Q$):** $Q = \rho \cdot \bar{v}$ (Density $\times$ Average Velocity).

---

## Key Findings & Performance Highlights

1. **Memory Locality & False Sharing:**
   * Eliminating false sharing in OpenMP cell-state updates yields up to a **3.4x speedup** over naïve parallel loops (`traffic_omp_bad`).
2. **OpenMP Chunk Scheduling:**
   * A `static` schedule with chunk sizes aligned to L1 cache lines (64 bytes / 16 integer cells) provides optimal cache locality for regular 1D domain structures.
3. **MPI Communication Overhead:**
   * For road sizes $L < 10^5$, OpenMP outperforms MPI due to the absence of explicit halo exchange network latency.
   * For $L \ge 10^7$, MPI demonstrates linear strong scaling across distributed nodes by distributing memory footprint requirements.
