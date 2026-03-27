#!/bin/sh
# TAP test: builtin APNG frame_no sequence stays loop-local and monotonic.

set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n";
    exit 0
}

. "${TOP_SRCDIR}/tests/_lib/sh/common.sh"

echo "1..1"
set -v

image_apng="${TOP_SRCDIR}/tests/data/inputs/formats/apng_8x8_rgba_loop2.png"
expected_sequence="0:0
0:1
1:0
1:1"

trace_log=$(
    set +xv
    run_img2sixel --env SIXEL_TRACE_TOPIC=encode_handoff,apng_decode \
                  -Lbuiltin! -lauto -g \
                  "${image_apng}" -o/dev/null 2>&1
) || {
    echo "not ok" 1 - "builtin APNG trace run failed"
    exit 0
}

actual_sequence=$(
    printf '%s\n' "${trace_log}" | awk '
        /callback frame_no=/ && /handoff=/ {
            frame = $0
            loop = $0
            sub(/^.*frame_no=/, "", frame)
            sub(/ .*/, "", frame)
            sub(/^.*loop_no=/, "", loop)
            sub(/ .*/, "", loop)
            printf "%s:%s\n", loop, frame
        }'
)

test "${actual_sequence}" = "${expected_sequence}" || {
    echo "not ok" 1 - "builtin APNG frame_no sequence mismatch"
    exit 0
}

echo "ok" 1 - "builtin APNG frame_no sequence stays loop-local"
exit 0
