#!/usr/bin/env bash
# run_experiments.sh — execute the full parameter matrix and collect CSV results.
#
# Parameter matrix (100 total runs):
#   Workloads: 1, 2
#   Protocols: occ, 2pl
#   Threads:   1, 2, 4, 8, 16
#   Hotset prob: 0.1, 0.3, 0.5, 0.7, 0.9
#
# Fixed: --txns-per-thread 200, --hotset-size 10
#
# Usage: ./scripts/run_experiments.sh [BUILD_DIR]
#   BUILD_DIR defaults to "build"

set -euo pipefail

BUILD_DIR="${1:-build}"
BIN="${BUILD_DIR}/transaction_system"
RESULTS_DIR="results"
CSV="${RESULTS_DIR}/results.csv"
LATENCY_CSV="${RESULTS_DIR}/latency_samples.csv"

# Sanity check
if [[ ! -x "${BIN}" ]]; then
    echo "ERROR: Binary not found at ${BIN}. Build first:"
    echo "  cmake -B ${BUILD_DIR} && cmake --build ${BUILD_DIR} -j"
    exit 1
fi

mkdir -p "${RESULTS_DIR}"
# Remove stale CSV so headers are written fresh
rm -f "${CSV}" "${LATENCY_CSV}"

WORKLOADS=(1 2)
PROTOCOLS=(occ 2pl)
THREADS=(1 2 4 8 16)
HOTSET_PROBS=(0.1 0.3 0.5 0.7 0.9)
TXNS_PER_THREAD=200
HOTSET_SIZE=10

# Representative run configuration (for latency distribution plot)
REP_WORKLOAD=1
REP_PROTOCOL=occ
REP_THREADS=4
REP_HOTSET=0.7

total_runs=$(( ${#WORKLOADS[@]} * ${#PROTOCOLS[@]} * ${#THREADS[@]} * ${#HOTSET_PROBS[@]} ))
run_num=0

for W in "${WORKLOADS[@]}"; do
  for P in "${PROTOCOLS[@]}"; do
    for T in "${THREADS[@]}"; do
      for H in "${HOTSET_PROBS[@]}"; do
        run_num=$(( run_num + 1 ))
        DB_PATH="tmp_db_w${W}_${P}_t${T}_h${H}"

        echo "[${run_num}/${total_runs}] workload=${W} protocol=${P} threads=${T} hotset_prob=${H}"

        # Delete stale DB
        rm -rf "${DB_PATH}"

        # Build the common argument list
        ARGS=(
            --workload         "${W}"
            --protocol         "${P}"
            --threads          "${T}"
            --txns-per-thread  "${TXNS_PER_THREAD}"
            --hotset-size      "${HOTSET_SIZE}"
            --hotset-prob      "${H}"
            --db-path          "${DB_PATH}"
            --csv-output       "${CSV}"
        )

        # Add latency dump for the representative run
        if [[ "${W}" == "${REP_WORKLOAD}" && \
              "${P}" == "${REP_PROTOCOL}"  && \
              "${T}" == "${REP_THREADS}"   && \
              "${H}" == "${REP_HOTSET}" ]]; then
            ARGS+=(--dump-latencies "${LATENCY_CSV}")
        fi

        "${BIN}" "${ARGS[@]}"

        # Clean up temporary DB
        rm -rf "${DB_PATH}"
      done
    done
  done
done

echo ""
echo "All ${total_runs} runs complete."
echo "Results:          ${CSV}"
echo "Latency samples:  ${LATENCY_CSV}"
