#!/usr/bin/env bash
set -euo pipefail

ROOT="/cluster/YewParVCTest"
SEQVC_BIN="${ROOT}/apps/bnb/vertexcover/seqVC"
GRAPH_DIR="${ROOT}/test/vc-experiments"

OUTDIR="${ROOT}/results_vc_full"
LOGDIR="${OUTDIR}/logs"
mkdir -p "${LOGDIR}"

CSV="${OUTDIR}/seqvc_baseline.csv"
SEQ_TIMEOUT="120m"

# Get all graphs from directory
mapfile -t GRAPHS < <(ls "${GRAPH_DIR}"/*.clq | xargs -n1 basename)

die(){ echo "ERROR: $*" >&2; exit 1; }
need_exe(){ [[ -x "$1" ]] || die "Missing executable: $1"; }
    
extract_vc_size() {
  awk -F'=' '/Minimum Vertex Cover Size/ {gsub(/[[:space:]]/,"",$2); print $2; exit}' "$1" || true
}

extract_cpu_ms() {
  awk '/^cpu[[:space:]]*=/ {for(i=1;i<=NF;i++) if($i ~ /^[0-9]+$/){print $i; exit}}' "$1" || true
}

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

need_exe "${SEQVC_BIN}"

echo "graph,wall_ms,cpu_ms,vc_size,exit_code,log_file" > "${CSV}"

echo "=== Running seqVC baseline on all graphs ==="
for g in "${GRAPHS[@]}"; do
  GP="${GRAPH_DIR}/${g}"
  
  echo "[seqVC] ${g}"
  
  log="${LOGDIR}/${g}.seqvc.r1.log"

  IFS=',' read -r wall_ms exit_code < <(run_capture "${log}" \
    timeout "${SEQ_TIMEOUT}" \
      "${SEQVC_BIN}" --input-file "${GP}")

  vc_size="$(extract_vc_size "${log}")"
  cpu_ms="$(extract_cpu_ms "${log}")"
  
  echo "${g},${wall_ms},${cpu_ms},${vc_size},${exit_code},${log}" >> "${CSV}"
  
  # Show quick result
  if [[ "$exit_code" == "0" ]]; then
    echo "  ✓ Completed: ${wall_ms}ms, vc_size=${vc_size}"
  elif [[ "$exit_code" == "124" ]]; then
    echo "  ⏱ Timeout after 120m"
  else
    echo "  ✗ Failed with exit code ${exit_code}"
  fi
done

echo ""
echo "Done. Results: ${CSV}"
