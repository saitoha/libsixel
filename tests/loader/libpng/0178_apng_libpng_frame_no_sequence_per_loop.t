#!/bin/sh
# TAP test: APNG frame_no sequence stays loop-local and monotonic.

set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n";
    exit 0
}

test "${HAVE_LIBPNG-}" = 1 || {
    printf "1..0 # SKIP libpng is disabled in this build\n";
    exit 0
}


echo "1..1"
set -v

image_apng="${TOP_SRCDIR}/tests/data/inputs/formats/apng_8x8_rgba_loop2.png"
expected_sequence="0:0
0:1
1:0
1:1"

actual_sequence=$(
    set +xv
    ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
        --env SIXEL_TRACE_TOPIC=encode_handoff,apng_decode \
        -Llibpng! -lauto -g \
        "${image_apng}" -o/dev/null 2>&1 \
        | awk '/callback frame_no=.*handoff=/ {
            frame_no = $0
            sub(/^.*frame_no=/, "", frame_no)
            sub(/ .*/, "", frame_no)
            loop_no = $0
            sub(/^.*loop_no=/, "", loop_no)
            sub(/ .*/, "", loop_no)
            sub(/\r$/, "", frame_no)
            sub(/\r$/, "", loop_no)
            if (sequence != "") {
                sequence = sequence "\n"
            }
            sequence = sequence loop_no ":" frame_no
        }
        END {
            printf "%s", sequence
        }'
) || {
    echo "not ok" 1 - "libpng APNG trace run failed"
    exit 0
}

test "${actual_sequence}" = "${expected_sequence}" || {
    echo "not ok" 1 - "libpng APNG frame_no sequence mismatch"
    exit 0
}

echo "ok" 1 - "libpng APNG frame_no sequence stays loop-local"
exit 0
