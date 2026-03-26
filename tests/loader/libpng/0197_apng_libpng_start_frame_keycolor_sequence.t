#!/bin/sh
# Verify libpng APNG start-frame + keycolor keeps emitted frame_no sequence.

set -eux

test "${HAVE_LIBPNG-}" = 1 || {
    echo "1..0 # SKIP libpng support is disabled in this build"
    exit 0
}

test "${HAVE_IMG2SIXEL-}" = 1 || {
    echo "1..0 # SKIP img2sixel is disabled in this build"
    exit 0
}

. "${TOP_SRCDIR}/tests/_lib/sh/common.sh"

echo "1..2"
set -v

image_apng="${TOP_SRCDIR}/tests/data/inputs/formats/apng_8x8_rgba_loop2.png"
expected_sequence="0:0
1:0
1:1"

for start_frame_no in 1 -1; do
    trace_log=$(
        set +xv
        run_img2sixel --env SIXEL_TRACE_TOPIC=encode_handoff,apng_decode \
                      --env SIXEL_LOADER_ANIMATION_START_FRAME_NO="${start_frame_no}" \
                      --env SIXEL_LOADER_LIBPNG_USE_TRNS_KEYCOLOR=1 \
                      -Llibpng! -lauto -g \
                      "${image_apng}" -o/dev/null 2>&1
    ) || {
        echo "not ok" $([ "${start_frame_no}" = "1" ] && echo 1 || echo 2) \
             - "libpng APNG trace run failed (start=${start_frame_no})"
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
        if [ "${start_frame_no}" = "1" ]; then
            echo "ok 1 - libpng APNG start=1 keycolor sequence stays loop-local"
        else
            echo "ok 2 - libpng APNG start=-1 keycolor sequence stays loop-local"
        fi
    else
        if [ "${start_frame_no}" = "1" ]; then
            echo "not ok 1 - libpng APNG start=1 keycolor sequence mismatch"
        else
            echo "not ok 2 - libpng APNG start=-1 keycolor sequence mismatch"
        fi
    fi
done

exit 0
