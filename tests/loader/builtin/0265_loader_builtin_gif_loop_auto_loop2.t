#!/bin/sh
# TAP test: loop=auto respects NETSCAPE loop2 as two passes.

set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n"
    exit 0
}


printf '1..1\n'
set -v

input_loop2="${TOP_SRCDIR}/tests/data/inputs/formats/gif-anim-netscape-loop2.gif"
expected_twice="0:0
0:1
1:0
1:1"

trace_log=$(
    set +xv
    ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" --env SIXEL_TRACE_TOPIC=encode_handoff \
                  -Lbuiltin! -lauto -g \
                  "${input_loop2}" -o /dev/null 2>&1
) || {
    echo "not ok" 1 - "loop auto respects NETSCAPE loop2 trace run failed"
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

test "${actual_sequence}" = "${expected_twice}" || {
    echo "not ok" 1 - "loop auto respects NETSCAPE loop2 sequence mismatch"
    exit 0
}

echo "ok" 1 - "loop auto respects NETSCAPE loop2"

exit 0
