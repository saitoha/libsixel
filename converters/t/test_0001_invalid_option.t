#!/usr/bin/env bash
# Verify error handling for invalid img2sixel options.
set -euo pipefail
SCRIPT_DIR=$(cd "$(dirname "$0")" && pwd)
# shellcheck source=converters/t/common.t
source "${SCRIPT_DIR}/common.t"

TRACE_INVALID_OPTION=${TRACE_INVALID_OPTION:-}
# Optional tracing hook that records how long each img2sixel invocation takes
# when TRACE_INVALID_OPTION is non-empty.  Windows runners pay a heavy process
# startup cost, so tracking the number of sequential executions helps explain
# why this test frequently exceeds the default 30-second Meson timeout.
TRACE_CLOCK_IMPL=""
INVALID_OPTION_INVOCATIONS=0
INVALID_OPTION_TOTAL_MS=0
# Random hangs were reported on MSYS2 runners.  Guard each invocation with a
# watchdog so the test suite can terminate promptly and report the exact
# command that stalled instead of idling until the Meson timeout expires.
INVALID_OPTION_TIMEOUT_SECS=${INVALID_OPTION_TIMEOUT_SECS:-20}
# Windows CI occasionally stalls while bash waits for a native child PID
# that already terminated.  Allow the watchdog to delegate process control to
# python or perl so we can rely on their timeout primitives instead of bash's
# background job tracking on MSYS2.  Fall back to the shell watchdog when
# those interpreters are unavailable.
INVALID_OPTION_WATCHDOG_IMPL=""
IMG2SIXEL_CMD=()

clamp_watchdog_timeout() {
    if [[ -z ${INVALID_OPTION_TIMEOUT_SECS} ]]; then
        INVALID_OPTION_TIMEOUT_SECS=20
        return
    fi
    if [[ ! ${INVALID_OPTION_TIMEOUT_SECS} =~ ^[0-9]+$ ]]; then
        INVALID_OPTION_TIMEOUT_SECS=20
        return
    fi
    if (( INVALID_OPTION_TIMEOUT_SECS < 1 )); then
        INVALID_OPTION_TIMEOUT_SECS=1
    fi
}

invalid_option_trace_exit() {
    local exit_status

    exit_status=$?
    trap - EXIT
    if [[ -n ${TRACE_INVALID_OPTION} ]]; then
        trace_summary
    fi
    exit "${exit_status}"
}

clamp_watchdog_timeout
if [[ -n ${TRACE_INVALID_OPTION} ]]; then
    trap invalid_option_trace_exit EXIT
fi

build_img2sixel_cmd() {
    IMG2SIXEL_CMD=()
    if [[ -n ${WINE} ]]; then
        IMG2SIXEL_CMD+=("${WINE}")
    fi
    IMG2SIXEL_CMD+=("${IMG2SIXEL_PATH}")
    IMG2SIXEL_CMD+=("$@")
}

log_watchdog_command() {
    local arg

    if [[ ${#IMG2SIXEL_CMD[@]} -eq 0 ]]; then
        return
    fi

    printf '%s' "${IMG2SIXEL_CMD[0]}" >&2
    for arg in "${IMG2SIXEL_CMD[@]:1}"; do
        printf ' %s' "${arg}" >&2
    done
    printf '\n' >&2
}

timestamp_ms() {
    case ${TRACE_CLOCK_IMPL} in
        python3)
            python3 - <<'EOF'
import time
print(int(time.time() * 1000))
EOF
            ;;
        python)
            python - <<'EOF'
import time
print(int(time.time() * 1000))
EOF
            ;;
        perl)
            perl -MTime::HiRes=time -e 'printf("%d\n", (int)(time() * 1000))'
            ;;
        *)
            date +%s000
            ;;
    esac
}

format_milliseconds() {
    local millis
    local seconds
    local fraction

    millis=$1
    seconds=$((millis / 1000))
    fraction=$((millis % 1000))

    printf '%d.%03d' "${seconds}" "${fraction}"
}

trace_invalid_option() {
    local elapsed
    local label

    if [[ -z ${TRACE_INVALID_OPTION} ]]; then
        return
    fi

    elapsed=$(format_milliseconds "$1")
    label=$2

    printf '[trace][invalid-option] %s (elapsed %ss)\n' \
        "${label}" "${elapsed}" >&2
}

trace_summary() {
    local total

    if [[ -z ${TRACE_INVALID_OPTION} ]]; then
        return
    fi

    total=$(format_milliseconds "${INVALID_OPTION_TOTAL_MS}")
    printf '[trace][invalid-option] %u img2sixel invocations, %ss total\n' \
        "${INVALID_OPTION_INVOCATIONS}" "${total}" >&2
}

select_watchdog_impl() {
    if [[ -n ${INVALID_OPTION_WATCHDOG_IMPL} ]]; then
        return
    fi
    if command -v python3 >/dev/null 2>&1; then
        INVALID_OPTION_WATCHDOG_IMPL="python3"
        return
    fi
    if command -v python >/dev/null 2>&1; then
        INVALID_OPTION_WATCHDOG_IMPL="python"
        return
    fi
    if command -v perl >/dev/null 2>&1; then
        INVALID_OPTION_WATCHDOG_IMPL="perl"
        return
    fi
    INVALID_OPTION_WATCHDOG_IMPL="shell"
}

run_with_watchdog() {
    local timeout_secs
    local label
    local status
    local impl
    local pid
    local watchdog_pid
    local watchdog_status

    timeout_secs=$1
    label=$2
    shift 2

    select_watchdog_impl
    impl=${INVALID_OPTION_WATCHDOG_IMPL}
    status=0

    case ${impl} in
        python3|python)
            "${impl}" - "$timeout_secs" "$label" "$@" <<'PYCODE'
import subprocess
import sys

timeout = int(sys.argv[1])
label = sys.argv[2]
cmd = sys.argv[3:]

if not cmd:
    sys.stderr.write('[watchdog][invalid-option] missing command\n')
    sys.exit(1)

try:
    proc = subprocess.Popen(cmd)
except OSError as exc:
    sys.stderr.write('[watchdog][invalid-option] failed to launch %s: %s\n'
                     % (label, exc))
    sys.exit(127)

try:
    proc.wait(timeout=timeout)
except subprocess.TimeoutExpired:
    sys.stderr.write('[watchdog][invalid-option] %s stalled for %us; '
                     'terminating\n' % (label, timeout))
    proc.kill()
    try:
        proc.wait(timeout=5)
    except subprocess.TimeoutExpired:
        pass
    sys.exit(124)

sys.exit(proc.returncode)
PYCODE
            status=$?
            ;;
        perl)
            perl - "$timeout_secs" "$label" "$@" <<'PLCODE'
use strict;
use warnings;
use POSIX qw(:sys_wait_h EINTR);

my $timeout = shift @ARGV;
my $label = shift @ARGV;
if (!@ARGV) {
    print STDERR "[watchdog][invalid-option] missing command\n";
    exit 1;
}

my $pid = fork();
if (!defined $pid) {
    print STDERR "[watchdog][invalid-option] failed to fork $label\n";
    exit 127;
}

if ($pid == 0) {
    exec @ARGV or do {
        print STDERR "[watchdog][invalid-option] exec failed $label: $!\n";
        exit 127;
    };
}

local $SIG{ALRM} = sub {
    print STDERR "[watchdog][invalid-option] $label stalled for ${timeout}s; terminating\n";
    kill 'TERM', $pid;
    exit 124;
};

while (1) {
    alarm $timeout;
    my $wait = waitpid($pid, 0);
    my $status = $?;
    alarm 0;
    if ($wait == -1) {
        next if $! == EINTR;
        print STDERR "[watchdog][invalid-option] waitpid failed for $label: $!\n";
        exit 1;
    }
    if ($wait == $pid) {
        exit($status >> 8);
    }
}
PLCODE
            status=$?
            ;;
        *)
            "$@" &
            pid=$!
            (
                sleep "${timeout_secs}"
                if kill -0 "${pid}" 2>/dev/null; then
                    printf '[watchdog][invalid-option] %s stalled for %us; terminating\n' \
                        "${label}" "${timeout_secs}" >&2
                    kill "${pid}" 2>/dev/null || true
                    exit 124
                fi
                exit 0
            ) &
            watchdog_pid=$!

            if ! wait "${pid}"; then
                status=$?
            fi

            watchdog_status=0
            if kill -0 "${watchdog_pid}" 2>/dev/null; then
                kill "${watchdog_pid}" 2>/dev/null || true
            fi
            if ! wait "${watchdog_pid}"; then
                watchdog_status=$?
            fi

            if (( watchdog_status == 124 )); then
                status=124
            fi
            ;;
    esac

    return "${status}"
}

if [[ -n ${TRACE_INVALID_OPTION} ]]; then
    if command -v python3 >/dev/null 2>&1; then
        TRACE_CLOCK_IMPL="python3"
    elif command -v python >/dev/null 2>&1; then
        TRACE_CLOCK_IMPL="python"
    elif command -v perl >/dev/null 2>&1; then
        TRACE_CLOCK_IMPL="perl"
    else
        echo "TRACE_INVALID_OPTION requires python or perl" >&2
        TRACE_INVALID_OPTION=""
    fi
fi

echo '[test1] invalid option handling'

# Ensure an unreadable input file does not leave stray output.
invalid_file="${TMP_DIR}/testfile"
rm -f "${invalid_file}"
touch "${invalid_file}"
chmod a-r "${invalid_file}"
# Expect img2sixel to fail cleanly when the source is unreadable.
output_file="${TMP_DIR}/capture.$$"
build_img2sixel_cmd "${invalid_file}"
log_watchdog_command
if "${IMG2SIXEL_CMD[@]}" </dev/null >"${output_file}" 2>/dev/null; then
    :
fi
if [[ -s ${output_file} ]]; then
    echo 'img2sixel unexpectedly produced output for unreadable file' >&2
    rm -f "${output_file}"
    exit 1
fi
rm -f "${output_file}"
rm -f "${invalid_file}"

rm -f "${TMP_DIR}/invalid_filename"

expect_failure() {
    local output_file
    local start_ms
    local end_ms
    local elapsed_ms
    local label
    local status
    local label_args

    output_file=$(mktemp "${TMP_DIR}/capture.invalid.XXXXXX")
    label="img2sixel $*"
    build_img2sixel_cmd "$@"
    log_watchdog_command
    if [[ -n ${TRACE_INVALID_OPTION} ]]; then
        start_ms=$(timestamp_ms)
    else
        start_ms=0
    fi
    status=0
    # Windows builds may fall back to stdin when the input path is rejected.
    # Redirect /dev/null so native executables cannot block on console input
    # if they attempt to read from stdin after the path validation fails.
    if ! run_with_watchdog \
        "${INVALID_OPTION_TIMEOUT_SECS}" \
        "${label}" \
        "${IMG2SIXEL_CMD[@]}" </dev/null >"${output_file}" 2>/dev/null; then
        status=$?
    fi
    if (( status == 124 )); then
        label_args=${label#img2sixel }
        printf 'img2sixel watchdog aborted after %us: %s\n' \
            "${INVALID_OPTION_TIMEOUT_SECS}" "${label_args}" >&2
        rm -f "${output_file}"
        exit 1
    fi
    if [[ -s ${output_file} ]]; then
        label_args=${label#img2sixel }
        printf 'img2sixel unexpectedly produced output: %s\n' \
            "${label_args}" >&2
        rm -f "${output_file}"
        exit 1
    fi
    rm -f "${output_file}"

    if [[ -n ${TRACE_INVALID_OPTION} && ${start_ms} -ne 0 ]]; then
        end_ms=$(timestamp_ms)
        elapsed_ms=$((end_ms - start_ms))
        INVALID_OPTION_INVOCATIONS=$((INVALID_OPTION_INVOCATIONS + 1))
        INVALID_OPTION_TOTAL_MS=$((INVALID_OPTION_TOTAL_MS + elapsed_ms))
        trace_invalid_option "${elapsed_ms}" "${label}"
    fi
}

# Reject a missing input path.
expect_failure "${TMP_DIR}/invalid_filename"
# Reject a directory as input.
expect_failure "."
# Report an unknown dither option.
expect_failure -d invalid_option
# Report an unknown resize filter.
expect_failure -r invalid_option
# Report an unknown scaling mode.
expect_failure -s invalid_option
# Report an unknown tone adjustment mode.
expect_failure -t invalid_option
# Report an invalid width value.
expect_failure -w invalid_option
# Report an invalid height value.
expect_failure -h invalid_option
# Report an invalid format name.
expect_failure -f invalid_option
# Report an invalid quality preset.
expect_failure -q invalid_option
# Report an invalid layout option.
expect_failure -l invalid_option
# Report an invalid bits-per-pixel argument.
expect_failure -b invalid_option
# Report an invalid encoder tweak.
expect_failure -E invalid_option
# Report an invalid background colour string.
expect_failure -B invalid_option
# Reject a background colour missing one component.
expect_failure -B '#ffff' "${TOP_SRCDIR}/images/map8.png"
# Reject an overly long background colour specification.
expect_failure -B '#0000000000000' "${TOP_SRCDIR}/images/map8.png"
# Reject a malformed hex colour.
expect_failure -B '#00G'
# Reject an unknown named colour.
expect_failure -B test
# Reject an incomplete rgb: colour form.
expect_failure -B 'rgb:11/11'
# Reject the unsupported legacy width syntax.
expect_failure '-%'
# Reject a palette file that does not exist.
expect_failure -m "${TMP_DIR}/invalid_filename" "${TOP_SRCDIR}/images/snake.jpg"
# Reject mutually exclusive palette and encode flags.
expect_failure -p16 -e "${TOP_SRCDIR}/images/snake.jpg"
# Reject an invalid colour space index.
expect_failure -I -C0 "${TOP_SRCDIR}/images/snake.png"
# Reject incompatible inspect and palette options.
expect_failure -I -p8 "${TOP_SRCDIR}/images/snake.png"
# Reject conflicting palette size and terminal preset.
expect_failure -p64 -bxterm256 "${TOP_SRCDIR}/images/snake.png"
# Reject 8-bit output when palette dump is requested.
expect_failure -8 -P "${TOP_SRCDIR}/images/snake.png"
