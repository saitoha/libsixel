#!/bin/sh
# Verify builtin APNG start=1 + keycolor keeps emitted frame_no sequence.

set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n"
    exit 0
}

. "${TOP_SRCDIR}/tests/_lib/sh/common.sh"

echo "1..1"
set -v

image_apng="${TOP_SRCDIR}/tests/data/inputs/formats/apng_8x8_rgba_loop2.png"
expected_sequence="0:0
1:0
1:1"
trace_log=$(
    set +xv
    run_img2sixel --env SIXEL_TRACE_TOPIC=encode_handoff,apng_decode \
                  --env SIXEL_LOADER_ANIMATION_START_FRAME_NO=1 \
                  --env SIXEL_LOADER_LIBPNG_USE_TRNS_KEYCOLOR=1 \
                  -Lbuiltin! -lauto -g \
                  "${image_apng}" -o/dev/null 2>&1
) || {
    echo "not ok 1 - builtin APNG trace run failed (start=1)"
    exit 0
}

actual_sequence=$(
    printf '%s\n' "${trace_log}" | awk '
        /callback frame_no=/ && /handoff=/ {
            frame = ""
            loop = ""
            for (i = 1; i <= NF; ++i) {
                token = $i
                if (token ~ /^frame_no=/) {
                    sub(/^frame_no=/, "", token)
                    frame = token
                } else if (token ~ /^loop_no=/) {
                    sub(/^loop_no=/, "", token)
                    loop = token
                }
            }
            if (frame != "" && loop != "") {
                print loop ":" frame
            }
        }'
)

if [ "${actual_sequence}" = "${expected_sequence}" ]; then
    echo "ok 1 - builtin APNG start=1 keycolor sequence stays loop-local"
else
    echo "not ok 1 - builtin APNG start=1 keycolor sequence mismatch"
fi

exit 0
