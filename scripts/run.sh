#!/usr/bin/env bash
# Interactive runner for cpu-sim with clear menus.
# Works with bash; no nounset to avoid "unbound variable" on empty input.
set -eo pipefail

# --- Config ---------------------------------------------------------------
TRACE_DEFAULT="traces/branch_demo.trace"
BUILD_DIR="build"
BIN="$BUILD_DIR/bin/cpu-sim"
DATA_DIR="data"

# Keys the C++ binary expects
PRED_KEYS=("static_nt" "static_t" "1bit" "2bit" "tournament")
# Pretty names for the menu
PRED_LABELS=(
  "Static (Always Not Taken)"
  "Static (Always Taken)"
  "One-bit predictor"
  "Two-bit predictor"
  "Tournament predictor"
)
# Slugs for filenames
PRED_SLUGS=("static_nt" "static_t" "one_bit" "two_bit" "tournament")

# --- Helpers --------------------------------------------------------------
die(){ echo "ERROR: $*" >&2; exit 1; }
repo_root(){ cd "$(dirname "${BASH_SOURCE[0]}")/.."; pwd; }

ensure_bash() {
  if [ -z "${BASH_VERSION:-}" ]; then
    die "This script requires bash. Run:  bash scripts/run.sh"
  fi
}

build() {
  local root; root="$(repo_root)"
  mkdir -p "$root/$BUILD_DIR"
  cmake -S "$root" -B "$root/$BUILD_DIR" -DCMAKE_BUILD_TYPE=Release >/dev/null
  cmake --build "$root/$BUILD_DIR" -j >/dev/null
}

run_sim() {
  local trace="$1" pred_key="$2" pred_slug="$3" fwd="$4"
  local root; root="$(repo_root)"
  local ftag; ftag=$([ "$fwd" = "ON" ] && echo "operand_fw_on" || echo "operand_fw_off")
  local base; base="$(basename "$trace" .trace)"
  local out="$root/$DATA_DIR/${base}__${ftag}__predictor_${pred_slug}.csv"

  mkdir -p "$root/$DATA_DIR"
  echo "→ Running | Forwarding: $fwd | Predictor: $pred_key | Trace: $trace"
  if [ "$fwd" = "ON" ]; then
    "$root/$BIN" --trace "$root/$trace" --predictor "$pred_key" --out "$out"
  else
    "$root/$BIN" --trace "$root/$trace" --predictor "$pred_key" --no-forwarding --out "$out"
  fi
  if command -v realpath >/dev/null 2>&1; then
    echo "   CSV: $(realpath --relative-to="$root" "$out")"
  else
    echo "   CSV: ${out#$root/}"
  fi
}

menu_forwarding() {
  # print menu to stderr; only the chosen value to stdout
  echo "Operand Forwarding:" >&2
  echo "  1) ON" >&2
  echo "  2) OFF" >&2
  local choice
  while true; do
    read -rp "Select forwarding [1-2]: " choice
    case "$choice" in
      1) echo "ON"; return 0;;
      2) echo "OFF"; return 0;;
      *) echo "Invalid choice. Please enter 1 or 2." >&2;;
    esac
  done
}

menu_predictor() {
  # print menu to stderr; only the chosen index to stdout
  echo "Branch Predictor:" >&2
  local i
  for i in "${!PRED_LABELS[@]}"; do
    printf "  %d) %s\n" "$((i+1))" "${PRED_LABELS[$i]}" >&2
  done
  local choice
  while true; do
    read -rp "Select predictor [1-${#PRED_LABELS[@]}]: " choice
    if [[ "$choice" =~ ^[1-9][0-9]*$ ]] && (( choice>=1 && choice<=${#PRED_KEYS[@]} )); then
      echo $((choice-1))   # stdout: index
      return 0
    fi
    echo "Invalid choice. Enter a number between 1 and ${#PRED_LABELS[@]}." >&2
  done
}

usage(){
cat <<'USAGE'
Usage:
  scripts/run.sh                         # interactive menu
  scripts/run.sh -t traces/X.trace       # interactive with selected trace
  scripts/run.sh --all                   # run all predictors with FWD ON and OFF
  scripts/run.sh --pred 2bit --fwd on [-t traces/X.trace]
  scripts/run.sh --list                  # list predictor keys

Output:
  data/<tracebase>__operand_fw_<on|off>__predictor_<slug>.csv
Predictor keys:
  static_nt, static_t, 1bit, 2bit, tournament
USAGE
}

# --- Args -----------------------------------------------------------------
TRACE="$TRACE_DEFAULT"
MODE="interactive"
PRED=""
FWD=""

while (( "$#" )); do
  case "$1" in
    -t|--trace) TRACE="$2"; shift 2;;
    --pred)     PRED="$2"; shift 2;;
    --fwd)      FWD="$(echo "$2" | tr '[:lower:]' '[:upper:]')"; shift 2;;
    --all)      MODE="all"; shift;;
    --list)     printf "%s\n" "${PRED_KEYS[@]}"; exit 0;;
    -h|--help)  usage; exit 0;;
    *)          echo "Unknown arg: $1"; usage; exit 1;;
  esac
done

# --- Run ------------------------------------------------------------------
ensure_bash
ROOT="$(repo_root)"
cd "$ROOT"
[ -f "$TRACE" ] || die "Trace not found: $TRACE"
build
[ -x "$BIN" ] || die "Binary missing: $BIN"

case "$MODE" in
  all)
    for i in "${!PRED_KEYS[@]}"; do
      run_sim "$TRACE" "${PRED_KEYS[$i]}" "${PRED_SLUGS[$i]}" "ON"
      run_sim "$TRACE" "${PRED_KEYS[$i]}" "${PRED_SLUGS[$i]}" "OFF"
    done
    ;;

  interactive)
    echo "Trace: $TRACE"
    FWD_CHOICE="$(menu_forwarding)"
    IDX="$(menu_predictor)"
    run_sim "$TRACE" "${PRED_KEYS[$IDX]}" "${PRED_SLUGS[$IDX]}" "$FWD_CHOICE"
    ;;

  *)
    # direct mode
    if [[ -z "$PRED" || -z "$FWD" ]]; then usage; exit 1; fi
    case "$FWD" in ON|OFF) ;; *) die "--fwd must be on|off";; esac
    FOUND=-1
    for i in "${!PRED_KEYS[@]}"; do
      [[ "${PRED_KEYS[$i]}" == "$PRED" ]] && FOUND=$i && break
    done
    (( FOUND >= 0 )) || die "Unknown predictor: $PRED (use --list)"
    run_sim "$TRACE" "$PRED" "${PRED_SLUGS[$FOUND]}" "$FWD"
    ;;
esac
