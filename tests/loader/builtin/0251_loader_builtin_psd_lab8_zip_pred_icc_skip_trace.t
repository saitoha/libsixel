#!/bin/sh
# Verify Lab ZIP+Prediction path keeps ICC-skip trace behavior.

set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n"
    exit 0
}

. "${TOP_SRCDIR}/tests/_lib/sh/common.sh"

echo "1..1"
set -v

input_psd="${TOP_SRCDIR}/tests/data/inputs/formats/stbi_minimal_lab8_zip_pred_icc.psd"

trace_log=$(set +xv; run_img2sixel -v -Lbuiltin:cms=auto! \
    "${input_psd}" -o /dev/null 2>&1 || true)

case "${trace_log}" in
    *"builtin PSD: skipping embedded ICC conversion for Lab custom decode path"*)
        echo "ok" 1 - "Lab ZIP+Prediction ICC skip trace is preserved"
        ;;
    *)
        echo "not ok" 1 - "Lab ZIP+Prediction ICC skip trace is missing"
        exit 0
        ;;
esac

exit 0
