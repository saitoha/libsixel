#!/bin/sh
# Verify 3-channel PSD with ICC does not emit false ICC failure trace when --bgcolor is set.

set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n"
    exit 0
}


echo "1..1"
set -v

input_psd="${TOP_SRCDIR}/tests/data/inputs/formats/snake-64-embedded-esrgb.psd"

trace_log=$(set +xv; ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" -v -L builtin:cms_engine=auto! \
    -B "#112233" "${input_psd}" -o /dev/null 2>&1 || true)

case "${trace_log}" in
    *"libsixel: trying builtin loader"*)
        ;;
    *)
        echo "not ok" 1 - "builtin loader was not attempted"
        exit 0
        ;;
esac

case "${trace_log}" in
    *"libsixel: loader builtin succeeded"*)
        ;;
    *)
        echo "not ok" 1 - "builtin loader did not finish successfully"
        exit 0
        ;;
esac

case "${trace_log}" in
    *"libsixel: builtin PSD: embedded ICC conversion failed"*)
        echo "not ok" 1 - "3-channel PSD emitted false ICC conversion failure trace with bgcolor"
        exit 0
        ;;
    *)
        ;;
esac

echo "ok" 1 - "3-channel PSD with bgcolor avoids false ICC failure trace"
exit 0
