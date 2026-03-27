#!/bin/sh
# Verify Bitmap 1-bit + ZIP prediction is explicitly unsupported.

set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n"
    exit 0
}


echo "1..1"
set -v

input_psd="${TOP_SRCDIR}/tests/data/inputs/formats/stbi_minimal_bitmap1_zip_pred.psd"

trace_log=$(set +xv; ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" -L builtin! "${input_psd}" -o /dev/null 2>&1 || true)

case "${trace_log}" in
    *"builtin PSD: unsupported compression (3) for Bitmap 1-bit"*)
        echo "ok" 1 - "Bitmap 1-bit ZIP prediction is explicitly unsupported"
        ;;
    *)
        echo "not ok" 1 - "Bitmap ZIP+Prediction policy trace is missing"
        exit 0
        ;;
esac

exit 0
