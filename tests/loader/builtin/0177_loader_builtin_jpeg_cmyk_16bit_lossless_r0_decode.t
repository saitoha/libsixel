#!/bin/sh
# Verify builtin loader decodes 16-bit CMYK lossless JPEG without fallback.

set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n"
    exit 0
}

. "${TOP_SRCDIR}/tests/_lib/sh/common.sh"

echo "1..1"
set -v

input_jpeg="${TOP_SRCDIR}/tests/data/inputs/formats/snake-jpeg-16bit-cmyk-lossless.jpg"

trace_log=$(set +xv; run_img2sixel -v -L builtin! \
    "${input_jpeg}" -o /dev/null 2>&1 || true)

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
        echo "not ok" 1 - "builtin loader failed to decode 16-bit CMYK lossless JPEG"
        exit 0
        ;;
esac

echo "ok" 1 - "builtin decodes 16-bit CMYK lossless JPEG"
exit 0
