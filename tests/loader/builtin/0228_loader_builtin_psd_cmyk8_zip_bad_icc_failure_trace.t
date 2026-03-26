#!/bin/sh
# Verify CMYK ZIP path keeps bad-ICC failure trace behavior.

set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n"
    exit 0
}

. "${TOP_SRCDIR}/tests/_lib/sh/common.sh"

echo "1..1"
set -v

input_psd="${TOP_SRCDIR}/tests/data/inputs/formats/stbi_minimal_cmyk8_zip_bad_icc.psd"

trace_log=$(set +xv; run_img2sixel -v -Lbuiltin:cms=auto! \
    "${input_psd}" -o /dev/null 2>&1 || true)

case "${trace_log}" in
    *"builtin PSD: embedded ICC conversion failed"*)
        echo "ok" 1 - "CMYK ZIP bad ICC failure trace is preserved"
        ;;
    *)
        echo "not ok" 1 - "CMYK ZIP bad ICC failure trace is missing"
        exit 0
        ;;
esac

exit 0
