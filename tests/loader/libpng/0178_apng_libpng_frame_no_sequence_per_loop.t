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

trace_log=$(
    set +xv
    ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
        --env SIXEL_TRACE_TOPIC=encode_handoff,apng_decode \
        -Llibpng! -lauto -g \
        "${image_apng}" -o/dev/null 2>&1
) || {
    echo "not ok" 1 - "libpng APNG trace run failed"
    exit 0
}

actual_sequence=""
parse_log="${trace_log}"
while :; do
    case "${parse_log}" in
        *"callback frame_no="*"handoff="*)
            callback_part=${parse_log#*callback frame_no=}
            frame_no=${callback_part%% *}
            loop_part=${callback_part#*loop_no=}
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
            parse_log=${callback_part#*handoff=}
            ;;
        *)
            break
            ;;
    esac
done

test "${actual_sequence}" = "${expected_sequence}" || {
    echo "not ok" 1 - "libpng APNG frame_no sequence mismatch"
    exit 0
}

echo "ok" 1 - "libpng APNG frame_no sequence stays loop-local"
exit 0
