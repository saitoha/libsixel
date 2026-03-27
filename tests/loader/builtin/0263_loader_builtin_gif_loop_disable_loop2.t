#!/bin/sh
# TAP test: loop=disable ignores NETSCAPE loop2 and emits one pass.

set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n"
    exit 0
}

. "${TOP_SRCDIR}/tests/_lib/sh/common.sh"

printf '1..1\n'
set -v

input_loop2="${TOP_SRCDIR}/tests/data/inputs/formats/gif-anim-netscape-loop2.gif"
expected_once="0:0
0:1"

trace_log=$(
    set +xv
    run_img2sixel --env SIXEL_TRACE_TOPIC=encode_handoff \
                  -Lbuiltin! -ldisable -g \
                  "${input_loop2}" -o /dev/null 2>&1
) || {
    echo "not ok" 1 - "loop disable ignores NETSCAPE loop2 trace run failed"
    exit 0
}

actual_sequence="$(printf '%s\n' "${trace_log}" | awk '
    /callback frame_no=/ && /handoff=/ {
        frame = $0
        loop = $0
        sub(/^.*frame_no=/, "", frame)
        sub(/ .*/, "", frame)
        sub(/^.*loop_no=/, "", loop)
        sub(/ .*/, "", loop)
        printf "%s:%s\n", loop, frame
    }')"

test "${actual_sequence}" = "${expected_once}" || {
    echo "not ok" 1 - "loop disable ignores NETSCAPE loop2 sequence mismatch"
    exit 0
}

echo "ok" 1 - "loop disable ignores NETSCAPE loop2"

exit 0
