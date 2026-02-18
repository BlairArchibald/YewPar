#!/usr/bin/env bash
set -euo pipefail

# Parameter sweep script for optimized vertex cover with narrow search trees
# Uses SMALLER spawn depths and budgets to force earlier work distribution

ROOT="/cluster/YewParVCTest"
APP="${ROOT}/build/apps/bnb/vertexcover/vertexcover-16"
GRAPH_DIR="${ROOT}/test/vc-experiments"

OUTDIR="${ROOT}/results_param_sweep"
LOGDIR="${OUTDIR}/logs"
mkdir -p "${LOGDIR}"

# Output CSV
CSV="${OUTDIR}/param_sweep_results.csv"

# MPI configuration
NUM_NODES=7  # Using 7 nodes (avoiding fatanode-09)
THREADS_PER_NODE=32
HOSTFILE="${ROOT}/hosts_param_sweep.txt"

# Test instances (mix of difficulties)
INSTANCES=(
  "hamming8-4.clq"           # Fast, good for testing
  "johnson16-2-4.clq"        # Small
  "brock200_1.clq"           # Medium
  "san200_0.9_1.clq"         # Medium-hard
  "gen200_p0.9_44.clq"       # Hard (known to parallelize well)
)

# SMALL spawn depths for narrow trees (force early spawning)
SPAWN_DEPTHS=(1 2 3 4 5)

# SMALL budgets for narrow trees (spawn more frequently)
BUDGETS=(10 25 50 100 250 500)

# Timeout for each run
TIMEOUT="10m"

die(){ echo "ERROR: $*" >&2; exit 1; }
[[ -x "$APP" ]] || die "Missing executable: $APP"

# CSV header
echo "instance,skeleton,param_name,param_value,nodes,threads,runtime_ms,vc_size,local_steals,dist_steals,failed_local,failed_dist,exit_code" > "${CSV}"

extract_value() {
  grep -oP "$1[[:space:]]*[:=][[:space:]]*\K[0-9]+" "$2" | head -1 || echo "0"
}

sum_hpx_counter() {
  # Sum HPX counter values across all localities
  # Format: /workstealing{locality#N/total}/path,1,time,[s],VALUE
  local counter_name="$1"
  local log_file="$2"
  grep "$counter_name" "$log_file" 2>/dev/null | grep -oP ',\K[0-9]+$' | awk '{s+=$1} END {print s+0}' || echo "0"
}

run_test() {
  local instance="$1"
  local skeleton="$2"
  local param_name="$3"
  local param_value="$4"
  
  local base_name="${instance%.clq}_${skeleton}_${param_name}${param_value}"
  local log_file="${LOGDIR}/${base_name}.log"
  
  echo "[${skeleton}] ${instance} (${param_name}=${param_value})"
  
  local cmd="timeout ${TIMEOUT} mpirun -np ${NUM_NODES} --hostfile ${HOSTFILE} --map-by node --mca orte_base_help_aggregate 0"
  cmd+=" ${APP} --skeleton ${skeleton} -f ${GRAPH_DIR}/${instance} --hpx:threads=${THREADS_PER_NODE}"
  
  # Add skeleton-specific parameters
  if [[ "$skeleton" == "depthbounded" ]]; then
    cmd+=" --spawn-depth ${param_value}"
  elif [[ "$skeleton" == "budget" ]]; then
    cmd+=" --backtrack-budget ${param_value}"
  fi
  
  # Add HPX counters for work distribution metrics
  if [[ "$skeleton" == "stacksteal" ]]; then
    cmd+=" --hpx:print-counter=/workstealing/SearchManager/distributedSteals"
  else
    cmd+=" --hpx:print-counter=/workstealing/depthpool/distributedSteals"
  fi
  cmd+=" --hpx:print-counter-interval=0"
  
  local start_ms=$(date +%s%3N)
  set +e
  eval "$cmd" > "$log_file" 2>&1
  local exit_code=$?
  set -e
  local end_ms=$(date +%s%3N)
  local elapsed_ms=$((end_ms - start_ms))
  
  # Extract metrics
  local runtime_ms=$(extract_value "cpu" "$log_file")
  local vc_size=$(extract_value "Minimum Vertex Cover Size" "$log_file")
  local local_steals=$(extract_value "Local Steals" "$log_file")
  local failed_local=$(extract_value "Failed Local Steals" "$log_file")
  local failed_dist=$(extract_value "Failed Distributed Steals" "$log_file")
  
  # Get distributed steals from HPX counter (aggregated across all localities)
  local dist_steals
  if [[ "$skeleton" == "stacksteal" ]]; then
    dist_steals=$(sum_hpx_counter "SearchManager/distributedSteals" "$log_file")
  else
    dist_steals=$(sum_hpx_counter "depthpool/distributedSteals" "$log_file")
  fi
  
  # Use wall time if cpu time not found (timeout case)
  [[ "$runtime_ms" == "0" ]] && runtime_ms=$elapsed_ms
  
  echo "${instance},${skeleton},${param_name},${param_value},${NUM_NODES},${THREADS_PER_NODE},${runtime_ms},${vc_size},${local_steals},${dist_steals},${failed_local},${failed_dist},${exit_code}" >> "${CSV}"
}

echo "=== Parameter Sweep: Optimized Vertex Cover ==="
echo "Strategy: Small spawn depths/budgets for narrow search trees"
echo "Instances: ${#INSTANCES[@]}"
echo "Output: ${CSV}"
echo ""

# Test 1: StackStealing baseline (no chunking)
echo "=== StackStealing (baseline) ==="
for instance in "${INSTANCES[@]}"; do
  run_test "$instance" "stacksteal" "chunked" "false"
done

# Test 2: StackStealing with chunking
echo ""
echo "=== StackStealing (chunked) ==="
for instance in "${INSTANCES[@]}"; do
  base_name="${instance%.clq}_stacksteal_chunked"
  log_file="${LOGDIR}/${base_name}.log"
  
  echo "[stacksteal-chunked] ${instance}"
  
  cmd="timeout ${TIMEOUT} mpirun -np ${NUM_NODES} --hostfile ${HOSTFILE} --map-by node --mca orte_base_help_aggregate 0"
  cmd+=" ${APP} --skeleton stacksteal --chunked -f ${GRAPH_DIR}/${instance} --hpx:threads=${THREADS_PER_NODE}"
  cmd+=" --hpx:print-counter=/workstealing/SearchManager/distributedSteals --hpx:print-counter-interval=0"
  
  start_ms=$(date +%s%3N)
  set +e
  eval "$cmd" > "$log_file" 2>&1
  exit_code=$?
  set -e
  end_ms=$(date +%s%3N)
  elapsed_ms=$((end_ms - start_ms))
  
  runtime_ms=$(extract_value "cpu" "$log_file")
  vc_size=$(extract_value "Minimum Vertex Cover Size" "$log_file")
  local_steals=$(extract_value "Local Steals" "$log_file")
  dist_steals=$(sum_hpx_counter "SearchManager/distributedSteals" "$log_file")
  failed_local=$(extract_value "Failed Local Steals" "$log_file")
  failed_dist=$(extract_value "Failed Distributed Steals" "$log_file")
  
  [[ "$runtime_ms" == "0" ]] && runtime_ms=$elapsed_ms
  
  echo "${instance},stacksteal,chunked,true,${NUM_NODES},${THREADS_PER_NODE},${runtime_ms},${vc_size},${local_steals},${dist_steals},${failed_local},${failed_dist},${exit_code}" >> "${CSV}"
done

# Test 3: DepthBounded with SMALL spawn depths
echo ""
echo "=== DepthBounded (small spawn depths: ${SPAWN_DEPTHS[*]}) ==="
for instance in "${INSTANCES[@]}"; do
  for depth in "${SPAWN_DEPTHS[@]}"; do
    run_test "$instance" "depthbounded" "spawn_depth" "$depth"
  done
done

# Test 4: Budget with SMALL budgets
echo ""
echo "=== Budget (small budgets: ${BUDGETS[*]}) ==="
for instance in "${INSTANCES[@]}"; do
  for budget in "${BUDGETS[@]}"; do
    run_test "$instance" "budget" "budget" "$budget"
  done
done

echo ""
echo "=== Parameter Sweep Complete ==="
echo "Results: ${CSV}"
echo "Logs: ${LOGDIR}"
echo ""
echo "Analysis suggestions:"
echo "1. Plot steals vs param_value to find optimal spawning point"
echo "2. Compare runtime vs steals - more steals should mean better speedup"
echo "3. Identify 'sweet spot' where enough work is distributed without overhead"
