CC      := gcc
CSTD    := -std=c11
WARN    := -Wall -Wextra -Wno-unused-result
OPT     := -O3 -march=native -funroll-loops -fno-omit-frame-pointer
OMPFLAG := -fopenmp
CFLAGS  := $(CSTD) $(WARN) $(OPT)
LDFLAGS :=

SRC     := src
BIN     := bin

BINARIES := $(BIN)/traffic_serial $(BIN)/traffic_omp $(BIN)/traffic_omp_bad \
            $(BIN)/traffic_serial_prof $(BIN)/traffic_omp_prof $(BIN)/traffic_mpi

.PHONY: all clean distclean dirs

all: dirs $(BINARIES)

dirs:
	@mkdir -p $(BIN) results plots

$(BIN)/traffic_serial: $(SRC)/traffic_serial.c $(SRC)/common.h
	$(CC) $(CFLAGS) -o $@ $< $(LDFLAGS)

$(BIN)/traffic_omp: $(SRC)/traffic_omp.c $(SRC)/common.h
	$(CC) $(CFLAGS) $(OMPFLAG) -o $@ $< $(LDFLAGS)

$(BIN)/traffic_omp_bad: $(SRC)/traffic_omp_bad.c $(SRC)/common.h
	$(CC) $(CFLAGS) $(OMPFLAG) -o $@ $< $(LDFLAGS)

# Builds con -pg para gprof
$(BIN)/traffic_serial_prof: $(SRC)/traffic_serial.c $(SRC)/common.h
	$(CC) $(CSTD) $(WARN) -O2 -pg -fno-omit-frame-pointer -g -o $@ $< $(LDFLAGS)

$(BIN)/traffic_omp_prof: $(SRC)/traffic_omp.c $(SRC)/common.h
	$(CC) $(CSTD) $(WARN) -O2 -pg -fno-omit-frame-pointer -g $(OMPFLAG) -o $@ $< $(LDFLAGS)

$(BIN)/traffic_mpi: $(SRC)/traffic_mpi.c $(SRC)/common.h
	mpicc $(CSTD) $(WARN) $(OPT) -o $@ $< $(LDFLAGS)


clean:
	rm -rf $(BIN)

distclean: clean
	rm -rf results/*.csv plots/*.png gmon.out
