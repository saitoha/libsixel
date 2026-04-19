#!/bin/sh
# TAP test: libwebp pipeline emits cancel stop trace on SIGINT.

set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n"
    exit 0
}

test "${HAVE_WEBP-}" = 1 || {
    printf "1..0 # SKIP libwebp loader is unavailable\n"
    exit 0
}

test "${SIXEL_ENABLE_THREADS-0}" = 1 || {
    printf "1..0 # SKIP thread support is disabled\n"
    exit 0
}

test "${HAVE_EMSCRIPTEN_H-0}" = 1 && {
    printf "1..0 # SKIP emscripten runtime may exit before stop trace\n"
    exit 0
}

test "${HAVE_WINDOWS_H-0}" = 1 && {
    printf "1..0 # SKIP signal trace check is unavailable on Windows\n"
    exit 0
}

test "${SIXEL_TSAN_BUILD-no}" = "yes" && test "${HAVE_APPKIT-0}" = 1 && {
    printf "1..0 # SKIP macOS TSan can stall SIGINT pipeline trace timing\n"
    exit 0
}

build_os="${RUNTIME_ENV_BUILD_OS-unknown}"
test "${build_os}" != "${build_os#haiku}" && {
    printf "1..0 # SKIP Haiku signal timing is unstable for this trace test\n"
    exit 0
}

echo "1..1"
set -v

input_webp="${TOP_SRCDIR}/tests/data/inputs/formats/animated-lossless-8x8-2frame-loop2-min.webp"
# Keep the common case fast with an early SIGINT.
# If the child is still alive after a short grace period, apply the
# previous long watchdog window as a fallback before hard-killing.

set +x
trace_summary=$(
    {
        ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
            --env "SIXEL_THREADS=4" \
            --env "SIXEL_TRACE_TOPIC=encode_handoff" \
            -Llibwebp! -lforce "${input_webp}" 2>&1 >/dev/null &
        pid=$!

        (
            sleep 0.2
            kill -0 "${pid}" 2>/dev/null || exit 0
            sleep 1
            kill -INT "${pid}" 2>/dev/null || true
            wait_limit=40
            test -n "${SIXEL_RUNTIME-}" && wait_limit=200
            while test "${wait_limit}" -gt 0; do
                kill -0 "${pid}" 2>/dev/null || {
                    exit 0
                }
                sleep 0.05
                wait_limit=$((wait_limit - 1))
            done
            kill -0 "${pid}" 2>/dev/null || exit 0
            kill -KILL "${pid}" 2>/dev/null || true
            exit 42
        ) >/dev/null 2>&1 &
        watchdog_pid=$!

        sleep 0.02
        kill -INT "${pid}" 2>/dev/null || true

        wait "${pid}" 2>/dev/null || true
        kill "${watchdog_pid}" 2>/dev/null || true
        wait "${watchdog_pid}" 2>/dev/null || true
        printf "__WATCHDOG_STATUS__=%s\n" "$?"
    }
)

timeout_flag=0
handoff_flag=0
stop_flag=0

trace_remainder=${trace_summary#*__WATCHDOG_STATUS__=42}
test "${trace_remainder}" != "${trace_summary}" && timeout_flag=1

trace_remainder=${trace_summary#*event=callback_handoff_decide handoff=pipeline}
test "${trace_remainder}" != "${trace_summary}" && handoff_flag=1

trace_remainder=${trace_summary#*event=pipeline_stop}
test "${trace_remainder}" != "${trace_summary}" && stop_flag=1

set -x

test "${timeout_flag}" = "0" || {
    echo "not ok" 1 - "libwebp pipeline did not stop after SIGINT"
    exit 0
}

test "${handoff_flag}" = "1" || {
    echo "not ok" 1 - "libwebp pipeline handoff trace missing"
    exit 0
}

test "${stop_flag}" = "1" || {
    echo "not ok" 1 - "libwebp pipeline stop reason trace missing"
    exit 0
}

echo "ok" 1 - "libwebp pipeline emits cancel stop trace on SIGINT"
exit 0
