#!/bin/sh
# Verify PSD CMYK 16-bit is explicitly unsupported.

set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n"
    exit 0
}

. "${TOP_SRCDIR}/tests/_lib/sh/common.sh"

echo "1..1"
set -v

input_psd="${TOP_SRCDIR}/tests/data/inputs/formats/stbi_minimal_cmyk16_raw.psd"

trace_log=$(set +xv; run_img2sixel -L builtin! "${input_psd}" -o /dev/null 2>&1 || true)

case "${trace_log}" in
    *"builtin PSD: unsupported bit depth (16) for CMYK"*)
        echo "ok" 1 - "CMYK 16-bit unsupported policy is explicit"
        ;;
    *)
        echo "not ok" 1 - "CMYK 16-bit unsupported trace is missing"
        exit 0
        ;;
esac

exit 0
