#!/usr/bin/env bash
#
# Local triage helper for third-party PoC samples.
#
# This script intentionally targets local-only investigation flows:
# - external PoCs stay outside the git index
# - each input is triaged via the existing AFL crash triage helper
# - a TSV summary is generated for stack-hash clustering

set -euo pipefail

usage() {
    cat >&2 <<'USAGE'
usage: fuzz-local-triage.sh \
  --input-dir <path> \
  --asan-runner <path> \
  --triage-dir <path> \
  [--nosan-runner <path>] \
  [--timeout <sec>] \
  [--label <name>] \
  [--limit <count>] \
  [--triage-script <path>] \
  [--strict]
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

input_dir=""
triage_dir=""
asan_runner=""
nosan_runner=""
timeout_sec=20
label="local-poc"
limit=0
strict=0

script_dir="$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)"
repo_root="$(CDPATH='' cd -- "${script_dir}/.." && pwd)"
triage_script="${repo_root}/.github/scripts/fuzz/triage_afl_crash.sh"

while [ "$#" -gt 0 ]; do
    case "$1" in
        --input-dir)
            input_dir="$2"
            shift 2
            ;;
        --triage-dir)
            triage_dir="$2"
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
        --timeout)
            timeout_sec="$2"
            shift 2
            ;;
        --label)
            label="$2"
            shift 2
            ;;
        --limit)
            limit="$2"
            shift 2
            ;;
        --triage-script)
            triage_script="$2"
            shift 2
            ;;
        --strict)
            strict=1
            shift
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

if [ -z "$input_dir" ] || [ -z "$triage_dir" ] || [ -z "$asan_runner" ]; then
    echo "error: --input-dir, --triage-dir, and --asan-runner are required" >&2
    usage
    exit 2
fi

if [ ! -d "$input_dir" ]; then
    echo "error: input directory was not found: $input_dir" >&2
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

case "$limit" in
    ''|*[!0-9]*)
        echo "error: --limit must be >= 0: $limit" >&2
        exit 2
        ;;
esac

mkdir -p "$triage_dir"
summary_tsv="$triage_dir/summary.tsv"
stack_tsv="$triage_dir/summary-by-stack.tsv"
printf 'index\tinput\tasan_reproduced\tstack_hash\tminimized\tjson\n' > "$summary_tsv"

index=0
failed=0

while IFS= read -r -d '' crash_file; do
    triage_output=""
    triage_json=""
    asan_repro=""
    stack_hash=""
    minimized_input=""
    rc=0

    index=$((index + 1))
    if [ "$limit" -gt 0 ] && [ "$index" -gt "$limit" ]; then
        break
    fi

    echo "[triage ${index}] ${crash_file}"

    set +e
    if [ -n "$nosan_runner" ]; then
        triage_output="$($triage_script \
            --crash-file "$crash_file" \
            --triage-dir "$triage_dir" \
            --asan-runner "$asan_runner" \
            --nosan-runner "$nosan_runner" \
            --timeout "$timeout_sec" \
            --label "$label" 2>&1)"
    else
        triage_output="$($triage_script \
            --crash-file "$crash_file" \
            --triage-dir "$triage_dir" \
            --asan-runner "$asan_runner" \
            --timeout "$timeout_sec" \
            --label "$label" 2>&1)"
    fi
    rc=$?
    set -e

    if [ "$rc" -ne 0 ]; then
        failed=$((failed + 1))
        echo "warning: triage failed rc=${rc} file=${crash_file}" >&2
        echo "$triage_output" >&2
        if [ "$strict" -eq 1 ]; then
            exit "$rc"
        fi
        continue
    fi

    triage_json="$(printf '%s\n' "$triage_output" | find_triage_json_from_output)"
    if [ -z "$triage_json" ] || [ ! -f "$triage_json" ]; then
        failed=$((failed + 1))
        echo "warning: triage json was not found for file=${crash_file}" >&2
        echo "$triage_output" >&2
        if [ "$strict" -eq 1 ]; then
            exit 1
        fi
        continue
    fi

    asan_repro="$(json_get_bool "$triage_json" "asan_reproduced")"
    stack_hash="$(json_get_string "$triage_json" "stack_hash_sha256")"
    minimized_input="$(json_get_string "$triage_json" "minimized_input")"

    if [ -z "$asan_repro" ]; then
        asan_repro="false"
    fi
    if [ -z "$stack_hash" ]; then
        stack_hash="(missing)"
    fi

    printf '%s\t%s\t%s\t%s\t%s\t%s\n' \
        "$index" "$crash_file" "$asan_repro" "$stack_hash" "$minimized_input" "$triage_json" \
        >> "$summary_tsv"
done < <(find "$input_dir" -type f -print0)

awk -F '\t' '
NR == 1 { next }
{
    hash = $4
    if (hash == "") {
        hash = "(missing)"
    }
    count[hash] += 1
    if (!(hash in sample)) {
        sample[hash] = $6
    }
}
END {
    print "count\tstack_hash\tsample_json"
    for (h in count) {
        printf "%d\t%s\t%s\n", count[h], h, sample[h]
    }
}
' "$summary_tsv" | sort -t $'\t' -k1,1nr > "$stack_tsv"

reproduced_count="$(awk -F '\t' 'NR > 1 && $3 == "true" { n += 1 } END { print n + 0 }' "$summary_tsv")"
unique_hash_count="$(awk -F '\t' 'NR > 1 { seen[$4] = 1 } END { print length(seen) + 0 }' "$summary_tsv")"
processed_count="$(awk 'END { print NR - 1 }' "$summary_tsv")"

echo "fuzz-local-triage: processed=${processed_count} reproduced=${reproduced_count} unique_stack_hashes=${unique_hash_count} failed=${failed}"
echo "fuzz-local-triage: summary=${summary_tsv}"
echo "fuzz-local-triage: stacks=${stack_tsv}"

if [ "$failed" -gt 0 ] && [ "$strict" -eq 1 ]; then
    exit 1
fi

exit 0
