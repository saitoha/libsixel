#!/bin/sh
# Verify PSD ZIP+Prediction stream truncation is malformed even with layer
# records present.

set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n"
    exit 0
}

. "${TOP_SRCDIR}/tests/_lib/sh/common.sh"

echo "1..1"
set -v

input_psd="${TOP_SRCDIR}/tests/data/inputs/formats/stbi_minimal_missing_composite_rgb_zip_pred_truncated.psd"

trace_log=$(set +xv; run_img2sixel -L builtin! "${input_psd}" -o /dev/null 2>&1 || true)

case "${trace_log}" in
    *"builtin PSD: malformed compressed channel stream"*)
        case "${trace_log}" in
            *"builtin PSD: unsupported file without merged/composite image"*)
                echo "not ok" 1 - "ZIP+Prediction truncation must not be treated as missing composite"
                exit 0
                ;;
            *)
                echo "ok" 1 - "ZIP+Prediction truncation with layer records is classified as malformed"
                ;;
        esac
        ;;
    *)
        echo "not ok" 1 - "ZIP+Prediction truncation malformed trace is missing"
        exit 0
        ;;
esac

exit 0
