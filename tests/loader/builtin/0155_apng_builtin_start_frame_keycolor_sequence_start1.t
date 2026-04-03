#!/bin/sh
# Verify builtin APNG start=1 + keycolor keeps emitted frame_no sequence.

set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n"
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
                  -Lbuiltin! -lauto -g \
                  "${image_apng}" -o/dev/null 2>&1
) || {
    echo "not ok 1 - builtin APNG trace run failed (start=1)"
    exit 0
}

actual_sequence=""
while IFS= read -r line; do
    parse_line="${line}"
    while :; do
        case "${parse_line}" in
            *"callback frame_no="*"handoff="*)
                callback_part=${parse_line#*callback frame_no=}
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
                parse_line=${callback_part#*handoff=}
                ;;
            *)
                break
                ;;
        esac
    done
done <<__TRACE_EOF__
${trace_log}
__TRACE_EOF__

test "${actual_sequence}" = "${expected_sequence}" || {
    echo "not ok 1 - builtin APNG start=1 keycolor sequence mismatch"
    exit 0
}

echo "ok 1 - builtin APNG start=1 keycolor sequence stays loop-local"

exit 0
