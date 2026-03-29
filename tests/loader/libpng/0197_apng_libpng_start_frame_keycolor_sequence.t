#!/bin/sh
# Verify libpng APNG start=1 + keycolor keeps emitted frame_no sequence.

set -eux

test "${HAVE_LIBPNG-}" = 1 || {
    echo "1..0 # SKIP libpng support is disabled in this build"
    exit 0
}

test "${HAVE_IMG2SIXEL-}" = 1 || {
    echo "1..0 # SKIP img2sixel is disabled in this build"
    exit 0
}


echo "1..1"
set -v

image_apng="${TOP_SRCDIR}/tests/data/inputs/formats/apng_8x8_rgba_loop2.png"
expected_sequence="0:0
1:0
1:1"
trace_log=$(
    set +xv
    ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" --env SIXEL_TRACE_TOPIC=encode_handoff,apng_decode \
                  --env SIXEL_LOADER_ANIMATION_START_FRAME_NO=1 \
                  --env SIXEL_LOADER_LIBPNG_USE_TRNS_KEYCOLOR=1 \
                  -Llibpng! -lauto -g \
                  "${image_apng}" -o/dev/null 2>&1
) || {
    echo "not ok 1 - libpng APNG trace run failed (start=1)"
    exit 0
}

actual_sequence=""
while IFS= read -r line; do
    case "${line}" in
        *"callback frame_no="*"handoff="*)
            frame_part=${line#*frame_no=}
            frame_no=${frame_part%% *}
            loop_part=${line#*loop_no=}
            loop_no=${loop_part%% *}
            case "${actual_sequence}" in
                "")
                    actual_sequence="${loop_no}:${frame_no}"
                    ;;
                *)
                    actual_sequence="${actual_sequence}
${loop_no}:${frame_no}"
                    ;;
            esac
            ;;
    esac
done <<__TRACE_EOF__
${trace_log}
__TRACE_EOF__

test "${actual_sequence}" = "${expected_sequence}" || {
    echo "not ok 1 - libpng APNG start=1 keycolor sequence mismatch"
    exit 0
}

    echo "ok 1 - libpng APNG start=1 keycolor sequence stays loop-local"


exit 0
