#!/usr/bin/env bash
set -euo pipefail

ROOT="/cluster/YewParVCTest"

# Base hostfile (one host per line)
HOSTFILE_BASE="${ROOT}/hosts.txt"

# Optional per-N hostfiles (hosts_1.txt, hosts_2.txt, hosts_4.txt, hosts_8.txt)
HOSTFILE_N_TEMPLATE="${ROOT}/hosts_%d.txt"

YWP_BIN="${ROOT}/build/apps/bnb/vertexcover/vertexcover-16"
SEQVC_BIN="${ROOT}/apps/bnb/vertexcover/seqVC"
GRAPH_DIR="${ROOT}/test/vc-experiments"

OUTDIR="${ROOT}/results_vc_full"
LOGDIR="${OUTDIR}/logs"
mkdir -p "${LOGDIR}"

CSV="${OUTDIR}/vc_experiments.csv"
COMPLETED_GRAPHS="${OUTDIR}/completed_sequential.txt"

# More expensive run now: scaling across nodes
REPEATS=1

# Threads per node for parallel runs
HPX_THREADS=32

# Scale across nodes
NODE_COUNTS=(1 2 4 8)

# Skeleton parameters
SPAWN_DEPTH=5
BACKTRACK_BUDGET=10000000

# Time limits
SEQ_TIMEOUT="120m"
PAR_TIMEOUT="120m"

# Get all graphs from directory
mapfile -t GRAPHS < <(ls "${GRAPH_DIR}"/*.clq | xargs -n1 basename)

# Sequential baselines first, then parallel skeletons for graphs that complete
SEQ_BASELINES=("seqvc" "seq")
PARALLEL_SKELETONS=("depthbounded" "budget" "stacksteal")

die(){ echo "ERROR: $*" >&2; exit 1; }
need_file(){ [[ -f "$1" ]] || die "Missing file: $1"; }
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

pick_hostfile() {
  local n="$1"
  local tmp_out="$2"

  local hf_n
  hf_n=$(printf "${HOSTFILE_N_TEMPLATE}" "${n}")

  if [[ -f "${hf_n}" ]]; then
    echo "${hf_n}"
    return 0
  fi

  head -n "${n}" "${HOSTFILE_BASE}" > "${tmp_out}"
  echo "${tmp_out}"
}

timeout_for() {
  local sk="$1"
  # With your request, both are 120m. Keep function in case you change later.
  if [[ "$sk" == "seq" ]]; then
    echo "$SEQ_TIMEOUT"
  else
    echo "$PAR_TIMEOUT"
  fi
}

threads_for() {
  local sk="$1"
  if [[ "$sk" == "seq" ]]; then
    echo "1"
  else
    echo "$HPX_THREADS"
  fi
}

# Sequential baseline: node=1 only. Parallel: run across NODE_COUNTS.
nodes_for() {
  local sk="$1"
  if [[ "$sk" == "seq" ]]; then
    echo "1"
  else
    printf "%s " "${NODE_COUNTS[@]}"
  fi
}

need_file "${HOSTFILE_BASE}"
need_exe "${YWP_BIN}"
need_exe "${SEQVC_BIN}"
for g in "${GRAPHS[@]}"; do need_file "${GRAPH_DIR}/${g}"; done

echo "experiment,graph,skeleton,nodes,hpx_threads,workers,spawn_depth,backtrack_budget,run,timeout,wall_ms,cpu_ms,vc_size,exit_code,log_file" > "${CSV}"
> "${COMPLETED_GRAPHS}"  # Clear completed graphs file

TMP_HOSTS="${OUTDIR}/hosts_tmp.txt"

# Phase 1: Run sequential baselines for all graphs
echo "=== Phase 1: Sequential Baselines ==="
for g in "${GRAPHS[@]}"; do
  GP="${GRAPH_DIR}/${g}"

  for sk in "${SEQ_BASELINES[@]}"; do
    for r in $(seq 1 "${REPEATS}"); do
      echo "[Sequential] ${g} baseline=${sk} run=${r}"
      
      log="${LOGDIR}/${g}.${sk}.r${r}.log"

      if [[ "$sk" == "seqvc" ]]; then
        # Run sequential C++ implementation
        IFS=',' read -r wall_ms exit_code < <(run_capture "${log}" \
          timeout "${SEQ_TIMEOUT}" \
            "${SEQVC_BIN}" --input-file "${GP}")
      else
        # Run YewPar seq skeleton
        IFS=',' read -r wall_ms exit_code < <(run_capture "${log}" \
          timeout "${SEQ_TIMEOUT}" \
            "${YWP_BIN}" --input-file "${GP}" \
              --skeleton seq \
              --hpx:threads 1)
      fi

      vc_size="$(extract_vc_size "${log}")"
      cpu_ms="$(extract_cpu_ms "${log}")"
      echo "SEQ,${g},${sk},1,1,0,${SPAWN_DEPTH},${BACKTRACK_BUDGET},${r},${SEQ_TIMEOUT},${wall_ms},${cpu_ms},${vc_size},${exit_code},${log}" >> "${CSV}"
      
      # Track graphs that completed successfully (exit_code 0)
      if [[ "$exit_code" == "0" ]] && [[ "$sk" == "seq" ]]; then
        echo "${g}" >> "${COMPLETED_GRAPHS}"
      fi
    done
  done
done

# Get unique list of completed graphs
sort -u "${COMPLETED_GRAPHS}" -o "${COMPLETED_GRAPHS}"
mapfile -t COMPLETED < "${COMPLETED_GRAPHS}"

echo ""
echo "=== Phase 2: Parallel Skeletons for Completed Graphs ==="
echo "Graphs that completed: ${COMPLETED[@]}"
echo ""

# Phase 2: Run parallel skeletons only for graphs that completed sequentially
for g in "${COMPLETED[@]}"; do
  GP="${GRAPH_DIR}/${g}"

  for sk in "${PARALLEL_SKELETONS[@]}"; do
    for n in "${NODE_COUNTS[@]}"; do
      HOSTFILE="$(pick_hostfile "${n}" "${TMP_HOSTS}")"
      THR="${HPX_THREADS}"
      WORKERS=$(( (HPX_THREADS - 1) * n ))

      for r in $(seq 1 "${REPEATS}"); do
        echo "[Parallel] ${g} skeleton=${sk} nodes=${n} threads=${THR} run=${r}"

        log="${LOGDIR}/${g}.ywp.${sk}.n${n}.t${THR}.r${r}.log"

        sk_args=(--skeleton "${sk}")
        case "${sk}" in
          depthbounded) sk_args+=(--spawn-depth "${SPAWN_DEPTH}") ;;
          budget)       sk_args+=(--backtrack-budget "${BACKTRACK_BUDGET}") ;;
          stacksteal)   ;; # No args - chunked=0 more stable per param sweep
        esac

        IFS=',' read -r wall_ms exit_code < <(run_capture "${log}" \
          timeout "${PAR_TIMEOUT}" \
            mpirun -np "${n}" --hostfile "${HOSTFILE}" \
              --map-by ppr:1:node --bind-to core \
              "${YWP_BIN}" --input-file "${GP}" \
                "${sk_args[@]}" \
                --hpx:threads "${THR}")

        vc_size="$(extract_vc_size "${log}")"
        cpu_ms="$(extract_cpu_ms "${log}")"
        echo "PAR,${g},${sk},${n},${THR},${WORKERS},${SPAWN_DEPTH},${BACKTRACK_BUDGET},${r},${PAR_TIMEOUT},${wall_ms},${cpu_ms},${vc_size},${exit_code},${log}" >> "${CSV}"
      done
    done
  done
done

rm -f "${TMP_HOSTS}" || true

echo ""
echo "Done."
echo "CSV:  ${CSV}"
echo "Logs: ${LOGDIR}"