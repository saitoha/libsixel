#!/usr/bin/env bash
#
# Post-process one AFL crash input.
#
# The script minimizes the crashing input, tries ASAN/NOSAN reproductions,
# collects a stack signature hash, and emits a machine-readable JSON report.

set -euo pipefail

timeout() {
    local timeout_duration
    local cmd_pid
    local watchdog_pid
    local cmd_status

    if [ "$#" -lt 2 ]; then
        echo "usage: timeout SECONDS command [args...]" >&2
        return 125
    fi

    timeout_duration="$1"
    shift

    (
        if command -v setsid >/dev/null 2>&1; then
            setsid "$@" &
        else
            "$@" &
        fi

        cmd_pid=$!

        (
            sleep "$timeout_duration"
            kill -TERM -"$cmd_pid" 2>/dev/null
            sleep 1
            kill -KILL -"$cmd_pid" 2>/dev/null
        ) &
        watchdog_pid=$!

        set +e
        wait "$cmd_pid"
        cmd_status=$?
        set -e
        kill "$watchdog_pid" 2>/dev/null || true
        wait "$watchdog_pid" 2>/dev/null || true
        case "$cmd_status" in
            137|143)
                exit 124
                ;;
            *)
                exit "$cmd_status"
                ;;
        esac
    )
    return $?
}

usage() {
    cat >&2 <<'USAGE'
usage: triage_afl_crash.sh \
  --crash-file <path> \
  --triage-dir <path> \
  --asan-runner <path> \
  [--nosan-runner <path>] \
  [--timeout <sec>] \
  [--afl-tmin <path>] \
  [--label <name>]
USAGE
}

json_escape() {
    printf '%s' "$1" | sed -e 's/\\/\\\\/g' -e 's/"/\\"/g'
}

run_repro() {
    local runner="$1"
    local input_file="$2"
    local log_file="$3"
    local timeout_sec="$4"
    local rc
    local reproduced

    set +e
    timeout "$timeout_sec" "$runner" < "$input_file" > "$log_file" 2>&1
    rc=$?
    set -e

    reproduced=0
    if [ "$rc" -ne 0 ] && [ "$rc" -ne 124 ]; then
        reproduced=1
    fi

    if grep -Eq 'AddressSanitizer|UndefinedBehaviorSanitizer|runtime error:|SUMMARY: AddressSanitizer|SEGV|ABRT|BUS|illegal instruction' "$log_file"; then
        reproduced=1
    fi

    printf '%s %s\n' "$rc" "$reproduced"
}

crash_file=""
triage_dir=""
asan_runner=""
nosan_runner=""
timeout_sec=20
label="afl-crash"
afl_tmin_bin="${AFL_TMIN:-afl-tmin}"

while [ "$#" -gt 0 ]; do
    case "$1" in
        --crash-file)
            crash_file="$2"
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
        --afl-tmin)
            afl_tmin_bin="$2"
            shift 2
            ;;
        --label)
            label="$2"
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

if [ -z "$crash_file" ] || [ -z "$triage_dir" ] || [ -z "$asan_runner" ]; then
    echo "error: --crash-file, --triage-dir, and --asan-runner are required" >&2
    usage
    exit 2
fi

if [ ! -f "$crash_file" ]; then
    echo "error: crash input was not found: $crash_file" >&2
    exit 1
fi

if [ ! -x "$asan_runner" ]; then
    echo "error: ASAN runner is not executable: $asan_runner" >&2
    exit 1
fi

case "$timeout_sec" in
    ''|*[!0-9]*|0)
        echo "error: --timeout must be a positive integer: $timeout_sec" >&2
        exit 2
        ;;
esac

mkdir -p "$triage_dir"

crash_base="$(basename "$crash_file")"
base="$triage_dir/${label}-${crash_base}"
orig_input="${base}.orig"
min_input="${base}.min"
tmin_log="${base}.tmin.log"
asan_log="${base}.asan.log"
nosan_log="${base}.nosan.log"
stack_frames="${base}.stack.txt"
json_path="${base}.json"

cp "$crash_file" "$orig_input"
cp "$crash_file" "$min_input"

tmin_status="skipped"
tmin_rc=0
if command -v "$afl_tmin_bin" >/dev/null 2>&1; then
    set +e
    "$afl_tmin_bin" -i "$orig_input" -o "$min_input" -m none -t 5000 \
        -- "$asan_runner" > "$tmin_log" 2>&1
    tmin_rc=$?
    set -e
    if [ "$tmin_rc" -eq 0 ]; then
        tmin_status="ok"
    else
        tmin_status="failed"
        cp "$orig_input" "$min_input"
    fi
else
    printf 'afl-tmin was not found: %s\n' "$afl_tmin_bin" > "$tmin_log"
fi

read -r asan_rc asan_repro <<<"$(run_repro "$asan_runner" "$min_input" "$asan_log" "$timeout_sec")"

nosan_rc=-1
nosan_repro=0
if [ -n "$nosan_runner" ] && [ -x "$nosan_runner" ]; then
    read -r nosan_rc nosan_repro <<<"$(run_repro "$nosan_runner" "$min_input" "$nosan_log" "$timeout_sec")"
else
    printf 'NOSAN runner was not configured.\n' > "$nosan_log"
fi

awk '
/^#[0-9]+ .* in / { print $4; n += 1; if (n >= 12) exit }
/SUMMARY: AddressSanitizer/ { print $3; n += 1; if (n >= 12) exit }
' "$asan_log" > "$stack_frames" || true

if [ ! -s "$stack_frames" ]; then
    sed -n '1,20p' "$asan_log" > "$stack_frames" || true
fi

if command -v sha256sum >/dev/null 2>&1; then
    stack_hash="$(sha256sum "$stack_frames" | awk '{print $1}')"
elif command -v shasum >/dev/null 2>&1; then
    stack_hash="$(shasum -a 256 "$stack_frames" | awk '{print $1}')"
else
    stack_hash="unavailable"
fi

timestamp="$(date -u +%Y-%m-%dT%H:%M:%SZ)"

cat > "$json_path" <<EOF
{
  "timestamp_utc": "$(json_escape "$timestamp")",
  "label": "$(json_escape "$label")",
  "crash_input": "$(json_escape "$orig_input")",
  "minimized_input": "$(json_escape "$min_input")",
  "afl_tmin_status": "$(json_escape "$tmin_status")",
  "afl_tmin_exit_code": $tmin_rc,
  "asan_runner": "$(json_escape "$asan_runner")",
  "asan_exit_code": $asan_rc,
  "asan_reproduced": $([ "$asan_repro" -eq 1 ] && printf 'true' || printf 'false'),
  "nosan_runner": "$([ -n "$nosan_runner" ] && json_escape "$nosan_runner" || printf '')",
  "nosan_exit_code": $nosan_rc,
  "nosan_reproduced": $([ "$nosan_repro" -eq 1 ] && printf 'true' || printf 'false'),
  "stack_hash_sha256": "$(json_escape "$stack_hash")",
  "logs": {
    "tmin": "$(json_escape "$tmin_log")",
    "asan": "$(json_escape "$asan_log")",
    "nosan": "$(json_escape "$nosan_log")"
  }
}
EOF

echo "triage-json: $json_path"
