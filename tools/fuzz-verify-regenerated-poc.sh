#!/usr/bin/env bash
#
# Verify that a regenerated PoC still reaches the same crash signature.
#
# Expected usage:
# 1) triage original external PoC and keep the generated JSON report.
# 2) regenerate a clean PoC from your own understanding.
# 3) run this script to compare stack-hash and reproduction status.

set -euo pipefail

usage() {
    cat >&2 <<'USAGE'
usage: fuzz-verify-regenerated-poc.sh \
  --baseline-json <path> \
  --candidate <path> \
  --asan-runner <path> \
  [--nosan-runner <path>] \
  [--work-dir <path>] \
  [--timeout <sec>] \
  [--label <name>] \
  [--triage-script <path>]
USAGE
}

resolve_runner_path() {
    local runner
    local candidate

    runner="$1"
    if [ -f "$runner" ] && head -n 1 "$runner" | grep -q '^#!'; then
        candidate="$(dirname "$runner")/.libs/$(basename "$runner")"
        if [ -x "$candidate" ]; then
            runner="$candidate"
        fi
    fi

    printf '%s\n' "$runner"
}

json_get_string() {
    local json_file
    local key

    json_file="$1"
    key="$2"
    sed -n -E "s/^[[:space:]]*\"${key}\"[[:space:]]*:[[:space:]]*\"([^\"]*)\".*/\\1/p" "$json_file" | head -n 1
}

json_get_bool() {
    local json_file
    local key

    json_file="$1"
    key="$2"
    sed -n -E "s/^[[:space:]]*\"${key}\"[[:space:]]*:[[:space:]]*(true|false).*/\\1/p" "$json_file" | head -n 1
}

find_triage_json_from_output() {
    sed -n 's/^triage-json: //p' | tail -n 1
}

baseline_json=""
candidate_file=""
asan_runner=""
nosan_runner=""
timeout_sec=20
label="regenerated"

script_dir="$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)"
repo_root="$(CDPATH='' cd -- "${script_dir}/.." && pwd)"
triage_script="${repo_root}/.github/scripts/fuzz/triage_afl_crash.sh"
work_dir="${repo_root}/.tmp/fuzz-regenerated-verify"

while [ "$#" -gt 0 ]; do
    case "$1" in
        --baseline-json)
            baseline_json="$2"
            shift 2
            ;;
        --candidate)
            candidate_file="$2"
            shift 2
            ;;
        --asan-runner)
            asan_runner="$2"
            shift 2
            ;;
        --nosan-runner)
            nosan_runner="$2"
            shift 2
            ;;
        --work-dir)
            work_dir="$2"
            shift 2
            ;;
        --timeout)
            timeout_sec="$2"
            shift 2
            ;;
        --label)
            label="$2"
            shift 2
            ;;
        --triage-script)
            triage_script="$2"
            shift 2
            ;;
        -h|--help)
            usage
            exit 0
            ;;
        *)
            echo "error: unknown argument: $1" >&2
            usage
            exit 2
            ;;
    esac
done

if [ -z "$baseline_json" ] || [ -z "$candidate_file" ] || [ -z "$asan_runner" ]; then
    echo "error: --baseline-json, --candidate, and --asan-runner are required" >&2
    usage
    exit 2
fi

if [ ! -f "$baseline_json" ]; then
    echo "error: baseline JSON was not found: $baseline_json" >&2
    exit 1
fi

if [ ! -f "$candidate_file" ]; then
    echo "error: candidate file was not found: $candidate_file" >&2
    exit 1
fi

if [ ! -x "$triage_script" ]; then
    echo "error: triage script is not executable: $triage_script" >&2
    exit 1
fi

asan_runner="$(resolve_runner_path "$asan_runner")"
if [ ! -x "$asan_runner" ]; then
    echo "error: ASAN runner is not executable: $asan_runner" >&2
    exit 1
fi

if [ -n "$nosan_runner" ]; then
    nosan_runner="$(resolve_runner_path "$nosan_runner")"
    if [ ! -x "$nosan_runner" ]; then
        echo "error: NOSAN runner is not executable: $nosan_runner" >&2
        exit 1
    fi
fi

case "$timeout_sec" in
    ''|*[!0-9]*|0)
        echo "error: --timeout must be a positive integer: $timeout_sec" >&2
        exit 2
        ;;
esac

baseline_hash="$(json_get_string "$baseline_json" "stack_hash_sha256")"
baseline_repro="$(json_get_bool "$baseline_json" "asan_reproduced")"

if [ -z "$baseline_hash" ]; then
    echo "error: baseline JSON does not contain stack_hash_sha256: $baseline_json" >&2
    exit 1
fi

mkdir -p "$work_dir"
run_id="$(date +%Y%m%d-%H%M%S)"
triage_dir="${work_dir}/${run_id}"
mkdir -p "$triage_dir"

triage_output=""
triage_json=""
if [ -n "$nosan_runner" ]; then
    triage_output="$($triage_script \
        --crash-file "$candidate_file" \
        --triage-dir "$triage_dir" \
        --asan-runner "$asan_runner" \
        --nosan-runner "$nosan_runner" \
        --timeout "$timeout_sec" \
        --label "$label")"
else
    triage_output="$($triage_script \
        --crash-file "$candidate_file" \
        --triage-dir "$triage_dir" \
        --asan-runner "$asan_runner" \
        --timeout "$timeout_sec" \
        --label "$label")"
fi

triage_json="$(printf '%s\n' "$triage_output" | find_triage_json_from_output)"
if [ -z "$triage_json" ] || [ ! -f "$triage_json" ]; then
    echo "error: candidate triage JSON was not generated" >&2
    echo "$triage_output" >&2
    exit 1
fi

candidate_hash="$(json_get_string "$triage_json" "stack_hash_sha256")"
candidate_repro="$(json_get_bool "$triage_json" "asan_reproduced")"

if [ -z "$candidate_hash" ]; then
    echo "error: candidate triage JSON does not contain stack_hash_sha256: $triage_json" >&2
    exit 1
fi

if [ -z "$candidate_repro" ]; then
    candidate_repro="false"
fi

if [ "$candidate_repro" != "true" ]; then
    echo "verify-result: FAIL"
    echo "reason: candidate did not reproduce under ASAN"
    echo "baseline_json: $baseline_json"
    echo "candidate_json: $triage_json"
    echo "baseline_stack_hash: $baseline_hash"
    echo "candidate_stack_hash: $candidate_hash"
    exit 1
fi

if [ "$candidate_hash" != "$baseline_hash" ]; then
    echo "verify-result: FAIL"
    echo "reason: stack hash mismatch"
    echo "baseline_json: $baseline_json"
    echo "candidate_json: $triage_json"
    echo "baseline_stack_hash: $baseline_hash"
    echo "candidate_stack_hash: $candidate_hash"
    echo "baseline_asan_reproduced: ${baseline_repro:-unknown}"
    echo "candidate_asan_reproduced: $candidate_repro"
    exit 1
fi

echo "verify-result: PASS"
echo "baseline_json: $baseline_json"
echo "candidate_json: $triage_json"
echo "stack_hash: $candidate_hash"
exit 0
