#!/usr/bin/env bash
set -euo pipefail

# Extended parameter sweep for DepthBounded and Budget skeletons
# DepthBounded: spawn_depth 5-20
# Budget: 500-100,000

ROOT="/cluster/YewParVCTest"
APP="${ROOT}/build/apps/bnb/vertexcover/vertexcover-16"
GRAPH_DIR="${ROOT}/test/vc-experiments"

OUTDIR="${ROOT}/results_param_sweep_extended"
LOGDIR="${OUTDIR}/logs"
mkdir -p "${LOGDIR}"

# Output CSV
CSV="${OUTDIR}/param_sweep_extended_results.csv"

# MPI configuration
NUM_NODES=7  # Using 7 nodes (avoiding fatanode-09)
THREADS_PER_NODE=32
HOSTFILE="${ROOT}/hosts_param_sweep.txt"

# Test instances (same as before)
INSTANCES=(
  "hamming8-4.clq"
  "johnson16-2-4.clq"
  "brock200_1.clq"
  "san200_0.9_1.clq"
  "gen200_p0.9_44.clq"
)

# Extended spawn depths for DepthBounded
SPAWN_DEPTHS=(5 6 7 8 9 10 11 12 13 14 15 16 17 18 19 20)

# Extended budgets for Budget skeleton (500 to 100,000)
BUDGETS=(500 1000 2500 5000 10000 25000 50000 75000 100000)

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
  cmd+=" --hpx:print-counter=/workstealing/depthpool/distributedSteals"
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
  local dist_steals=$(sum_hpx_counter "depthpool/distributedSteals" "$log_file")
  
  # Use wall time if cpu time not found (timeout case)
  [[ "$runtime_ms" == "0" ]] && runtime_ms=$elapsed_ms
  
  echo "${instance},${skeleton},${param_name},${param_value},${NUM_NODES},${THREADS_PER_NODE},${runtime_ms},${vc_size},${local_steals},${dist_steals},${failed_local},${failed_dist},${exit_code}" >> "${CSV}"
}

echo "=== Extended Parameter Sweep: DepthBounded and Budget ==="
echo "DepthBounded spawn depths: ${SPAWN_DEPTHS[*]}"
echo "Budget values: ${BUDGETS[*]}"
echo "Instances: ${#INSTANCES[@]}"
echo "Output: ${CSV}"
echo ""

# Test 1: DepthBounded with extended spawn depths (5-20)
echo "=== DepthBounded (spawn depths: ${SPAWN_DEPTHS[*]}) ==="
for instance in "${INSTANCES[@]}"; do
  for depth in "${SPAWN_DEPTHS[@]}"; do
    run_test "$instance" "depthbounded" "spawn_depth" "$depth"
  done
done

# Test 2: Budget with extended budgets (500-100,000)
echo ""
echo "=== Budget (budgets: ${BUDGETS[*]}) ==="
for instance in "${INSTANCES[@]}"; do
  for budget in "${BUDGETS[@]}"; do
    run_test "$instance" "budget" "budget" "$budget"
  done
done

echo ""
echo "=== Extended Parameter Sweep Complete ==="
echo "Results: ${CSV}"
echo "Logs: ${LOGDIR}"
echo ""
echo "Total tests run: $((${#INSTANCES[@]} * (${#SPAWN_DEPTHS[@]} + ${#BUDGETS[@]})))"
echo ""
echo "Analysis suggestions:"
echo "1. Compare DepthBounded performance across spawn_depth range"
echo "2. Identify Budget sweet spot between 500-100,000"
echo "3. Check if larger budgets improve distribution or just add overhead"
echo "4. Examine scaling behavior as parameters increase"
