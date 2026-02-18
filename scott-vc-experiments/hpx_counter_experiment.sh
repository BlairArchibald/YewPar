#!/usr/bin/env bash
set -euo pipefail

# HPX Performance Counter Experiment
# Run vertex cover on 8 nodes with harder instances to analyze task distribution
# across different skeletons using HPX performance counters

ROOT="/cluster/YewParVCTest"
HOSTFILE="${ROOT}/hosts_8.txt"
YWP_BIN="${ROOT}/build/apps/bnb/vertexcover/vertexcover-16"
GRAPH_DIR="${ROOT}/test/vc-experiments"
OUTDIR="${ROOT}/results_hpx_counters"
LOGDIR="${OUTDIR}/logs"
mkdir -p "${LOGDIR}"

CSV="${OUTDIR}/hpx_counter_results.csv"

# Fixed configuration
NODES=8
HPX_THREADS=32

# Skeleton parameters (based on param sweep optimal values)
SPAWN_DEPTH=3
BACKTRACK_BUDGET=100000

# Timeout for medium instances (should complete much faster)
TIMEOUT="10m"

# Select medium instances (should complete in minutes, not hours)
# Large enough to show meaningful task distribution, small enough to finish quickly
HARD_INSTANCES=(
  "gen200_p0.9_44.clq"  # 200 vertices - known to work at 8 nodes (~40s)
  "gen200_p0.9_55.clq"  # 200 vertices - easier variant
  "p_hat300-1.clq"      # 300 vertices - medium difficulty
  "p_hat300-2.clq"      # 300 vertices - medium difficulty
  "p_hat300-3.clq"      # 300 vertices - higher density
  "brock200_2.clq"      # 200 vertices - structured
  "C125.9.clq"          # 125 vertices - high density, manageable size
  "san200_0.9_1.clq"    # 200 vertices - high density
)

# Test all 3 skeletons to show distribution differences
SKELETONS=("depthbounded" "budget" "stacksteal")

# YewPar automatically registers and prints performance counters
# The logs will contain work distribution, stealing, and task statistics

die(){ echo "ERROR: $*" >&2; exit 1; }
need_file(){ [[ -f "$1" ]] || die "Missing file: $1"; }
need_exe(){ [[ -x "$1" ]] || die "Missing executable: $1"; }

# Validate setup
need_file "${HOSTFILE}"
need_exe "${YWP_BIN}"
for g in "${HARD_INSTANCES[@]}"; do 
  need_file "${GRAPH_DIR}/${g}"
done

# CSV header
echo "graph,skeleton,nodes,hpx_threads,spawn_depth,backtrack_budget,chunked,wall_ms,cpu_ms,vc_size,exit_code,log_file" > "${CSV}"

run_capture() {
  local logfile="$1"; shift
  local start_ms end_ms elapsed_ms exit_code
  start_ms=$(date +%s%3N)
  set +e
  "$@" >"$logfile" 2>&1
  exit_code=$?
  set -e
  end_ms=$(date +%s%3N)
  elapsed_ms=$((end_ms - start_ms))
  echo "${elapsed_ms},${exit_code}"
}

extract_vc_size() {
  awk -F'=' '/Minimum Vertex Cover Size/ {gsub(/[[:space:]]/,"",$2); print $2; exit}' "$1" || echo ""
}

extract_cpu_ms() {
  awk '/^cpu[[:space:]]*=/ {for(i=1;i<=NF;i++) if($i ~ /^[0-9]+$/){print $i; exit}}' "$1" || echo ""
}

echo "==================================================================="
echo "HPX Performance Counter Experiment"
echo "==================================================================="
echo "Configuration:"
echo "  Nodes: ${NODES}"
echo "  Threads per node: ${HPX_THREADS}"
echo "  Instances: ${#HARD_INSTANCES[@]} (expected: minutes to 10s of minutes)"
echo "  Skeletons: ${SKELETONS[@]}"
echo "  Timeout: ${TIMEOUT}"
echo "==================================================================="
echo ""

for g in "${HARD_INSTANCES[@]}"; do
  GP="${GRAPH_DIR}/${g}"
  
  for sk in "${SKELETONS[@]}"; do
    echo "[8-node HPX Counter] ${g} skeleton=${sk}"
    
    log="${LOGDIR}/${g%.clq}_${sk}_n${NODES}.log"
    
    # Build skeleton-specific arguments
    sk_args=(--skeleton "${sk}")
    chunked="N/A"
    
    case "${sk}" in
      depthbounded) 
        sk_args+=(--spawn-depth "${SPAWN_DEPTH}")
        ;;
      budget)
        sk_args+=(--backtrack-budget "${BACKTRACK_BUDGET}")
        ;;
      stacksteal)
        # Use chunked=0 (more stable per param sweep)
        chunked="0"
        ;;
    esac
    
    # Note: YewPar already registers and prints performance counters internally
    # via YewPar::registerPerformanceCounters() in main.cpp
    # Adding --hpx:print-counter causes MPI finalization conflicts
    # Counter data will be in the log output automatically
    
    # Run experiment
    IFS=',' read -r wall_ms exit_code < <(run_capture "${log}" \
      timeout "${TIMEOUT}" \
        mpirun -np "${NODES}" --hostfile "${HOSTFILE}" \
          --map-by ppr:1:node --bind-to core \
          "${YWP_BIN}" --input-file "${GP}" \
            "${sk_args[@]}" \
            --hpx:threads "${HPX_THREADS}")
    
    vc_size="$(extract_vc_size "${log}")"
    cpu_ms="$(extract_cpu_ms "${log}")"
    
    # Record results
    echo "${g},${sk},${NODES},${HPX_THREADS},${SPAWN_DEPTH},${BACKTRACK_BUDGET},${chunked},${wall_ms},${cpu_ms},${vc_size},${exit_code},${log}" >> "${CSV}"
    
    # Show immediate result
    if [[ "$exit_code" == "0" ]]; then
      echo "  ✓ Success: vc_size=${vc_size}, cpu=${cpu_ms}ms, wall=${wall_ms}ms"
    elif [[ "$exit_code" == "124" ]]; then
      echo "  ✗ TIMEOUT after ${TIMEOUT}"
    else
      echo "  ✗ FAILED: exit_code=${exit_code}"
    fi
    echo ""
  done
done

echo "==================================================================="
echo "Experiment Complete"
echo "==================================================================="
echo "Results CSV: ${CSV}"
echo "Log directory: ${LOGDIR}"
echo ""
echo "To analyze HPX counter data, check the log files for:"
echo "  - Thread utilization per locality"
echo "  - Idle rates (shows load balancing)"
echo "  - Work stealing statistics"
echo "  - Pending tasks distribution"
echo ""
echo "Example analysis command:"
echo "  grep 'threads.*count/cumulative' ${LOGDIR}/*.log"
echo "  grep 'idle-rate' ${LOGDIR}/*.log"
echo "  grep 'stolen' ${LOGDIR}/*.log"
