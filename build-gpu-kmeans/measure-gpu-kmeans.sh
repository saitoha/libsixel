#!/bin/sh
# Rebuild and measure in this worktree; never install or change Git refs.
set -eu

usage() {
    echo "Usage: $0 [--full] [--baseline INSTRUMENTED_COMMIT]"
    echo "  --full       Also run make staticcheck and the complete make check."
    echo "  --baseline   First measure an archived source snapshot inside this build."
    echo "JOBS (default 8) controls build/test parallelism, not concurrent benchmarks."
}

full=0
baseline=
while test "$#" -gt 0; do
    case "$1" in
        --full) full=1 ;;
        --baseline)
            shift
            test "$#" -gt 0 || { usage >&2; exit 2; }
            baseline=$1 ;;
        --help|-h) usage; exit 0 ;;
        *) usage >&2; exit 2 ;;
    esac
    shift
done

build=$(CDPATH='' cd -- "${0%/*}" && pwd -P)
root=$(CDPATH='' cd -- "$build/.." && pwd -P)
export PATH="$root/.local/bin:$PATH"
export LC_ALL=C
jobs=${JOBS:-8}
make_command=${MAKE:-make}
results="$build/measure-$(date +%Y%m%d-%H%M%S)-$$"
mkdir "$results"
mkdir "$results/tmp"
export TMPDIR="$results/tmp"
unset SIXEL_TRACE_TOPIC SIXEL_KMEANS_GPU_PROFILE

# Keep full command output for failures and copy calibration logs before the
# next pass overwrites them. Timing runs always execute one test at a time.
run_logged() {
    run_log=$1
    shift
    if "$@" >"$run_log" 2>&1; then
        return 0
    else
        run_status=$?
        cat "$run_log" >&2
        return "$run_status"
    fi
}

summarize() {
    awk -F '|' '
    function flush( p) {
        for (p in calls) {
            printf "KMPHASESUM|attempt=%d|phase=%s|dispatches=%d|gpu_ms=%.6f|gpu_mean_ms=%.6f|encode_ms=%.6f|submit_ms=%.6f|invalid_timestamps=%d|\n", attempt, p, calls[p], gpu[p], gpu[p]/calls[p], enc[p], subm[p], invalid[p]
        }
        split("", calls); split("", gpu); split("", enc)
        split("", subm); split("", invalid)
    }
    /^LSXKMTIME1\|/ { flush(); ++attempt }
    /^(KMCAL|LSXKMTIME1|LSXKMPHASE1|LSXKMCALL1|LSXKMRESTART1)\|/ { print }
    /^LSXKMSTAGE1\|/ {
        split("", v)
        for (i=2; i<=NF; ++i) { n=split($i, a, "="); if (n==2) v[a[1]]=a[2] }
        p=v["phase"] (v["noop"] ? "-noop" : "")
        ++calls[p]; enc[p]+=v["encode_ms"]; subm[p]+=v["submit_ms"]
        if (v["gpu_valid"]) gpu[p]+=v["gpu_ms"]; else ++invalid[p]
    }
    END { flush() }
    ' "$1"
}

{
    echo "KMMACHINE|model=$(sysctl -n hw.model 2>/dev/null || printf unavailable)|chip=$(sysctl -n machdep.cpu.brand_string 2>/dev/null || printf unavailable)|"
    echo "KMVERSION|revision=${KMEANS_MEASURE_REVISION:-$(git -C "$root" rev-parse HEAD)}|"
    sw_vers
    echo "KMRESULTS|path=$results|"
    echo "Untraced KMCAL is the throughput measurement; stages mode adds submission overhead."
} | tee "$results/machine.txt"

if test -n "$baseline"; then
    baseline=$(git -C "$root" rev-parse --verify "$baseline^{commit}")
    mkdir "$results/baseline-src"
    git -C "$root" archive --format=tar --output="$results/baseline.tar" "$baseline"
    tar -xf "$results/baseline.tar" -C "$results/baseline-src"
    echo "=== Baseline $baseline ==="
    KMEANS_MEASURE_REVISION=$baseline \
        sh "$results/baseline-src/build-gpu-kmeans/measure-gpu-kmeans.sh"
fi

if ! test -f "$build/Makefile"; then
    # Match the maintainer calibration build and avoid installing extensions.
    (cd "$build" && run_logged "$results/configure.log" "$root/configure" \
        --disable-quicklook-extension --disable-quicklook-preview \
        --without-coregraphics --disable-shared --enable-static BASH=/bin/sh)
fi
run_logged "$results/build.log" "$make_command" -C "$build" -j"$jobs"
# check_PROGRAMS are not built by the default all target.
run_logged "$results/test-build.log" "$make_command" -C "$build/tests" \
    -j"$jobs" test_runner

calibrated=0
for mode in untraced batch stages; do
    echo "=== Calibration: $mode ==="
    unset SIXEL_TRACE_TOPIC SIXEL_KMEANS_GPU_PROFILE
    if test "$mode" != untraced; then
        export SIXEL_TRACE_TOPIC=kmeans_backend
    fi
    if test "$mode" = stages; then
        export SIXEL_KMEANS_GPU_PROFILE=stages
    fi
    for test_name in 0068_snake_sse 0069_gradient_sse 0070_rare_sse; do
        echo "=== $test_name ($mode) ==="
        run_logged "$results/$mode-$test_name-make.log" \
            "$make_command" -C "$build/tests" -j1 check \
            "TESTS=palette/kmeans/$test_name.t"
        cp "$build/tests/palette/kmeans/$test_name.log" \
            "$results/$mode-$test_name.log"
        summarize "$results/$mode-$test_name.log"
        awk '/^ok [0-9]+.*# SKIP/ { print "KMSKIP|" $0 }' \
            "$results/$mode-$test_name.log"
        awk '/^# (TOTAL|PASS|SKIP|XFAIL|FAIL|XPASS|ERROR):/ { print }' \
            "$results/$mode-$test_name-make.log"
        if test "$mode" = untraced; then
            calibrated=$((calibrated + $(awk '/^KMCAL\|/ { ++n } END { print n+0 }' \
                "$results/$mode-$test_name.log")))
        fi
    done
done
unset SIXEL_TRACE_TOPIC SIXEL_KMEANS_GPU_PROFILE

echo "=== Entire kmeans suite ==="
suite=
for test_path in "$root"/tests/palette/kmeans/*.t; do
    suite="$suite palette/kmeans/${test_path##*/}"
done
run_logged "$results/kmeans.log" "$make_command" -C "$build" \
    -j"$jobs" check "TESTS=$suite"
awk '/^# (TOTAL|PASS|SKIP|XFAIL|FAIL|XPASS|ERROR):/ { print }' "$results/kmeans.log"
# Some TAP runners count a file with one skipped assertion as PASS. Report
# the device skips from actual TAP lines independently of that summary.
awk '/^ok [0-9]+.*# SKIP.*Metal/ { ++n }
    END { printf "KMDEVICE|skipped_assertions=%d|\n", n }' \
    "$build"/tests/palette/kmeans/*.log

if test "$full" -eq 1; then
    echo "=== Full staticcheck ==="
    run_logged "$results/staticcheck.log" "$make_command" -C "$build" \
        -j"$jobs" staticcheck
    tail -15 "$results/staticcheck.log"
    echo "=== Full check ==="
    run_logged "$results/check.log" "$make_command" -C "$build" -j"$jobs" check
    awk '/^# (TOTAL|PASS|SKIP|XFAIL|FAIL|XPASS|ERROR):/ { print }' "$results/check.log"
fi
printf 'Saved raw per-iteration timestamps and full logs: %s\n' "$results"
test "$calibrated" -eq 9 || {
    echo "Calibration unavailable: expected 9 KMCAL rows; got $calibrated. GPU skips are not measurements." >&2
    exit 77
}
