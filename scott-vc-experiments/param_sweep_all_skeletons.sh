#!/usr/bin/env bash
set -euo pipefail

ROOT="/cluster/YewParVCTest"
HOSTFILE_BASE="${ROOT}/hosts.txt"
YWP_BIN="${ROOT}/build/apps/bnb/vertexcover/vertexcover-16"
GRAPH_DIR="${ROOT}/test/vc-experiments"

OUTDIR="${ROOT}/results_param_sweep_fixed"
LOGDIR="${OUTDIR}/logs"
mkdir -p "${LOGDIR}"

CSV="${OUTDIR}/all_skeletons_param_sweep.csv"

REPEATS=1
HPX_THREADS=32
TIMEOUT="30m"

# Test graphs (small to medium for quick results)
GRAPHS=(
  "gen200_p0.9_44.clq"
  "gen200_p0.9_55.clq"
)

# Parameter ranges to test
SPAWN_DEPTHS=(2 3 5)
BACKTRACK_BUDGETS=(50000 100000 500000)
CHUNKED_VALUES=(0 1)
NODE_COUNTS=(1 2 4 8)

die(){ echo "ERROR: $*" >&2; exit 1; }
need_file(){ [[ -f "$1" ]] || die "Missing file: $1"; }
need_exe(){ [[ -x "$1" ]] || die "Missing executable: $1"; }

extract_vc_size() {
  awk -F'=' '/Minimum Vertex Cover Size/ {gsub(/[[:space:]]/,"",$2); print $2; exit}' "$1" || echo ""
}

extract_cpu_ms() {
  awk '/^cpu[[:space:]]*=/ {for(i=1;i<=NF;i++) if($i ~ /^[0-9]+$/){print $i; exit}}' "$1" || echo ""
}

run_capture() {
  local logfile="$1"; shift
  local start_ms end_ms elapsed_ms exit_code
  start_ms=$(date +%s%3N)
  set +e
  timeout "${TIMEOUT}" "$@" >"$logfile" 2>&1
  exit_code=$?
  set -e
  end_ms=$(date +%s%3N)
  elapsed_ms=$((end_ms - start_ms))
  echo "${elapsed_ms},${exit_code}"
}

pick_hostfile() {
  local n="$1"
  local tmp_hf="${OUTDIR}/hosts_${n}.txt"
  head -n "${n}" "${HOSTFILE_BASE}" > "${tmp_hf}"
  echo "${tmp_hf}"
}

need_file "${HOSTFILE_BASE}"
need_exe "${YWP_BIN}"

echo "skeleton,graph,nodes,hpx_threads,spawn_depth,backtrack_budget,chunked,run,wall_ms,cpu_ms,vc_size,exit_code,log_file" > "${CSV}"

echo "=== Parameter Sweep with Serialization Fix ==="
echo "Testing: DepthBounded, Budget, StackStealing"
echo "Graphs: ${GRAPHS[*]}"
echo "Spawn depths: ${SPAWN_DEPTHS[*]}"
echo "Backtrack budgets: ${BACKTRACK_BUDGETS[*]}"
echo "Node counts: ${NODE_COUNTS[*]}"
echo ""

# Calculate total runs
total_runs=0
for graph in "${GRAPHS[@]}"; do
  for nodes in "${NODE_COUNTS[@]}"; do
    # DepthBounded: spawn_depth x budget combinations
    for sd in "${SPAWN_DEPTHS[@]}"; do
      for bb in "${BACKTRACK_BUDGETS[@]}"; do
        total_runs=$((total_runs + 1))
      done
    done
    # Budget: spawn_depth x budget combinations
    for sd in "${SPAWN_DEPTHS[@]}"; do
      for bb in "${BACKTRACK_BUDGETS[@]}"; do
        total_runs=$((total_runs + 1))
      done
    done
    # StackStealing: chunked values only
    for chunked in "${CHUNKED_VALUES[@]}"; do
      total_runs=$((total_runs + 1))
    done
  done
done

echo "Total runs: ${total_runs} (estimated time: $((total_runs * 10 / 60)) - $((total_runs * 30 / 60)) minutes)"
echo ""

current_run=0

for graph in "${GRAPHS[@]}"; do
  graph_file="${GRAPH_DIR}/${graph}"
  need_file "${graph_file}"
  
  echo "=== Testing ${graph} ==="
  
  for nodes in "${NODE_COUNTS[@]}"; do
    hostfile=$(pick_hostfile "${nodes}")
    
    # Test DepthBounded
    for spawn_depth in "${SPAWN_DEPTHS[@]}"; do
      for budget in "${BACKTRACK_BUDGETS[@]}"; do
        current_run=$((current_run + 1))
        skeleton="depthbounded"
        log="${LOGDIR}/${graph%.clq}_${skeleton}_n${nodes}_sd${spawn_depth}_bb${budget}.log"
        
        echo "[${current_run}/${total_runs}] ${skeleton} | ${graph} | nodes=${nodes} sd=${spawn_depth} bb=${budget}"
        
        result=$(run_capture "${log}" \
          mpirun --hostfile "${hostfile}" -np "${nodes}" \
          "${YWP_BIN}" \
          --skeleton "${skeleton}" \
          --input-file "${graph_file}" \
          --hpx:threads="${HPX_THREADS}" \
          --spawn-depth="${spawn_depth}" \
          --backtrack-budget="${budget}")
        
        wall_ms=$(echo "${result}" | cut -d',' -f1)
        exit_code=$(echo "${result}" | cut -d',' -f2)
        vc_size=$(extract_vc_size "${log}")
        cpu_ms=$(extract_cpu_ms "${log}")
        
        echo "${skeleton},${graph},${nodes},${HPX_THREADS},${spawn_depth},${budget},0,1,${wall_ms},${cpu_ms},${vc_size},${exit_code},${log}" >> "${CSV}"
        
        if [[ "${exit_code}" -ne 0 ]]; then
          echo "  ⚠️  Exit code ${exit_code}"
        fi
      done
    done
    
    # Test Budget
    for spawn_depth in "${SPAWN_DEPTHS[@]}"; do
      for budget in "${BACKTRACK_BUDGETS[@]}"; do
        current_run=$((current_run + 1))
        skeleton="budget"
        log="${LOGDIR}/${graph%.clq}_${skeleton}_n${nodes}_sd${spawn_depth}_bb${budget}.log"
        
        echo "[${current_run}/${total_runs}] ${skeleton} | ${graph} | nodes=${nodes} sd=${spawn_depth} bb=${budget}"
        
        result=$(run_capture "${log}" \
          mpirun --hostfile "${hostfile}" -np "${nodes}" \
          "${YWP_BIN}" \
          --skeleton "${skeleton}" \
          --input-file "${graph_file}" \
          --hpx:threads="${HPX_THREADS}" \
          --spawn-depth="${spawn_depth}" \
          --backtrack-budget="${budget}")
        
        wall_ms=$(echo "${result}" | cut -d',' -f1)
        exit_code=$(echo "${result}" | cut -d',' -f2)
        vc_size=$(extract_vc_size "${log}")
        cpu_ms=$(extract_cpu_ms "${log}")
        
        echo "${skeleton},${graph},${nodes},${HPX_THREADS},${spawn_depth},${budget},0,1,${wall_ms},${cpu_ms},${vc_size},${exit_code},${log}" >> "${CSV}"
        
        if [[ "${exit_code}" -ne 0 ]]; then
          echo "  ⚠️  Exit code ${exit_code}"
        fi
      done
    done
    
    # Test StackStealing (no spawn_depth or budget params)
    for chunked in "${CHUNKED_VALUES[@]}"; do
      current_run=$((current_run + 1))
      skeleton="stacksteal"
      log="${LOGDIR}/${graph%.clq}_${skeleton}_n${nodes}_ch${chunked}.log"
      
      echo "[${current_run}/${total_runs}] ${skeleton} | ${graph} | nodes=${nodes} chunked=${chunked}"
      
      if [[ "${chunked}" -eq 1 ]]; then
        chunked_flag="--chunked"
      else
        chunked_flag=""
      fi
      
      result=$(run_capture "${log}" \
        mpirun --hostfile "${hostfile}" -np "${nodes}" \
        "${YWP_BIN}" \
        --skeleton "${skeleton}" \
        --input-file "${graph_file}" \
        --hpx:threads="${HPX_THREADS}" \
        ${chunked_flag})
      
      wall_ms=$(echo "${result}" | cut -d',' -f1)
      exit_code=$(echo "${result}" | cut -d',' -f2)
      vc_size=$(extract_vc_size "${log}")
      cpu_ms=$(extract_cpu_ms "${log}")
      
      # Use N/A for params that don't apply to stackstealing
      echo "${skeleton},${graph},${nodes},${HPX_THREADS},N/A,N/A,${chunked},1,${wall_ms},${cpu_ms},${vc_size},${exit_code},${log}" >> "${CSV}"
      
      if [[ "${exit_code}" -ne 0 ]]; then
        echo "  ⚠️  Exit code ${exit_code}"
      fi
    done
  done
done

echo ""
echo "=== Parameter Sweep Complete ==="
echo "Results: ${CSV}"
echo ""

# Summary analysis
echo "=== Best configurations per skeleton (by speedup) ==="
awk -F',' '
NR>1 && $12==0 {
  # Key: skeleton,graph,spawn_depth,backtrack_budget,chunked
  key=$1","$2","$5","$6","$7
  
  # Track 1-node baseline
  if ($3==1) {
    baseline[key]=$10  # cpu_ms
  }
  
  # Calculate speedup for multi-node runs
  if ($3>1 && baseline[key]>0) {
    speedup=baseline[key]/$10
    printf "%-12s %-25s n=%-2d sd=%-3s bb=%-7s ch=%s cpu=%6dms speedup=%.2fx\n", $1, $2, $3, $5, $6, $7, $10, speedup
  }
}' "${CSV}" | sort -t'=' -k2 -nr | head -20

echo ""
echo "Exit code summary:"
awk -F',' 'NR>1 {codes[$12]++} END {for(c in codes) printf "Exit %s: %d runs\n", c, codes[c]}' "${CSV}"
