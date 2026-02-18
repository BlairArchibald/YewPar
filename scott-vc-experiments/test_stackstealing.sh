#!/usr/bin/env bash
set -euo pipefail

ROOT="/cluster/YewParVCTest"
HOSTFILE_BASE="${ROOT}/hosts.txt"
YWP_BIN="${ROOT}/build/apps/bnb/vertexcover/vertexcover-16"
GRAPH_DIR="${ROOT}/test/vc-experiments"

OUTDIR="${ROOT}/results_stacksteal_test"
LOGDIR="${OUTDIR}/logs"
mkdir -p "${LOGDIR}"

CSV="${OUTDIR}/stacksteal_scaling.csv"

REPEATS=1
HPX_THREADS=32
TIMEOUT="30m"

GRAPHS=(
  "gen200_p0.9_44.clq"
  "gen200_p0.9_55.clq"
)

CHUNKED_VALUES=(0 1)  # 0=steal one task, 1=steal all available (--chunked flag)
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

echo "skeleton,graph,nodes,hpx_threads,chunked,run,wall_ms,cpu_ms,vc_size,exit_code,log_file" > "${CSV}"

echo "=== Testing StackStealing with v2 fixes ==="
echo "Graphs: ${GRAPHS[*]}"
echo "Chunked: ${CHUNKED_VALUES[*]}"
echo "Node counts: ${NODE_COUNTS[*]}"
echo ""

total_runs=0
for graph in "${GRAPHS[@]}"; do
  for nodes in "${NODE_COUNTS[@]}"; do
    for chunked in "${CHUNKED_VALUES[@]}"; do
      total_runs=$((total_runs + 1))
    done
  done
done

echo "Total runs: ${total_runs}"
echo ""

current_run=0
for graph in "${GRAPHS[@]}"; do
  graph_file="${GRAPH_DIR}/${graph}"
  need_file "${graph_file}"
  
  echo "=== Testing ${graph} ==="
  
  for nodes in "${NODE_COUNTS[@]}"; do
    hostfile=$(pick_hostfile "${nodes}")
    
    for chunked in "${CHUNKED_VALUES[@]}"; do
      current_run=$((current_run + 1))
      
      skeleton="stacksteal"
      log="${LOGDIR}/${graph%.clq}_${skeleton}_n${nodes}_ch${chunked}.log"
      
      echo "[${current_run}/${total_runs}] ${graph} | nodes=${nodes} chunked=${chunked}"
      
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
      
      echo "${skeleton},${graph},${nodes},${HPX_THREADS},${chunked},1,${wall_ms},${cpu_ms},${vc_size},${exit_code},${log}" >> "${CSV}"
      
      if [[ "${exit_code}" -ne 0 ]]; then
        echo "  ⚠️  Exit code ${exit_code} - checking log..."
        tail -5 "${log}" | sed 's/^/    /'
      fi
    done
  done
done

echo ""
echo "=== StackStealing Test Complete ==="
echo "Results: ${CSV}"
echo ""
echo "Scaling analysis (successful runs only):"
awk -F',' '
NR>1 && $10==0 {
  key=$2","$5
  if ($3==1) baseline[key]=$8
  speedup=baseline[key]/$8
  printf "%-25s nodes=%-2d chunked=%d wall=%6dms cpu=%6dms speedup=%.2fx\n", $2, $3, $5, $8, $9, speedup
}' "${CSV}"

echo ""
echo "Exit code summary:"
awk -F',' 'NR>1 {codes[$10]++} END {for(c in codes) printf "Exit %s: %d runs\n", c, codes[c]}' "${CSV}"
