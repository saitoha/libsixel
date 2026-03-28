#!/bin/sh
# Verify builtin PSD reports malformed layer/mask section when image data is
# absent and layer section metadata is inconsistent.

set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n"
    exit 0
}

echo "1..1"
set -v

input_psd="${TOP_SRCDIR}/tests/data/inputs/formats/stbi_minimal_missing_image_data_badlayer.psd"
trace_log=$(set +xv; ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" -L builtin! "${input_psd}" -o /dev/null 2>&1 || true)

case "${trace_log}" in
    *"builtin PSD: malformed layer/mask section"*)
        echo "ok" 1 - "broken layer metadata is reported as malformed"
        ;;
    *)
        echo "not ok" 1 - "malformed layer/mask trace is missing"
        exit 0
        ;;
esac

exit 0
