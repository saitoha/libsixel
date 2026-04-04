#!/bin/sh
# TAP test: builtin GIF pipeline emits cancel stop trace on SIGINT.

set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n"
    exit 0
}

test "${SIXEL_ENABLE_THREADS-0}" = 1 || {
    printf "1..0 # SKIP thread support is disabled\n"
    exit 0
}

test "${HAVE_WINDOWS_H-0}" = 1 && {
    printf "1..0 # SKIP signal trace check is unavailable on Windows\n"
    exit 0
}

echo "1..1"
set -v

input_gif="${TOP_SRCDIR}/tests/data/inputs/formats/gif-anim-netscape-loop0.gif"

trace_summary=$(
    {
        ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
            --env "SIXEL_THREADS=4" \
            --env "SIXEL_TRACE_TOPIC=encode_handoff" \
            -Lbuiltin! -lforce "${input_gif}" 2>&1 >/dev/null &
        pid=$!

        sleep 0.1
        kill -INT "${pid}" 2>/dev/null || true

        wait_limit=20
        while test "${wait_limit}" -gt 0; do
            kill -0 "${pid}" 2>/dev/null || {
                break
            }
            sleep 0.01
            wait_limit=$((wait_limit - 1))
        done

        kill -0 "${pid}" 2>/dev/null && {
            kill -KILL "${pid}" 2>/dev/null || true
            wait "${pid}" 2>/dev/null || true
            printf "__TIMEOUT__\n"
        }

        wait "${pid}" 2>/dev/null || true
    } | {
        saw_timeout=0
        saw_handoff=0
        saw_stop=0
        while IFS= read -r line; do
            case "${line}" in
                *__TIMEOUT__*)
                    saw_timeout=1
                    ;;
                *"event=callback_handoff_decide handoff=pipeline"*)
                    saw_handoff=1
                    ;;
                *"event=pipeline_stop"*)
                    saw_stop=1
                    ;;
            esac
        done
        printf "%s:%s:%s\n" "${saw_timeout}" "${saw_handoff}" "${saw_stop}"
    }
)

timeout_flag=${trace_summary%%:*}
trace_summary=${trace_summary#*:}
handoff_flag=${trace_summary%%:*}
stop_flag=${trace_summary##*:}

test "${timeout_flag}" = "0" || {
    echo "not ok" 1 - "builtin GIF pipeline did not stop after SIGINT"
    exit 0
}

test "${handoff_flag}" = "1" || {
    echo "not ok" 1 - "builtin GIF pipeline handoff trace missing"
    exit 0
}

test "${stop_flag}" = "1" || {
    echo "not ok" 1 - "builtin GIF pipeline stop reason trace missing"
    exit 0
}

echo "ok" 1 - "builtin GIF pipeline emits cancel stop trace on SIGINT"
exit 0
