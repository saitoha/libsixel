#!/bin/sh
# Verify PSD without merged/composite image is explicitly unsupported.

set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n"
    exit 0
}


echo "1..1"
set -v

input_psd="${TOP_SRCDIR}/tests/data/inputs/formats/stbi_minimal_missing_composite_rgb.psd"

trace_log=$(set +xv; ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" -L builtin! "${input_psd}" -o /dev/null 2>&1 || true)

case "${trace_log}" in
    *"builtin PSD: unsupported file without merged/composite image"*)
        echo "ok" 1 - "missing merged/composite image is deterministically rejected"
        ;;
    *)
        echo "not ok" 1 - "missing composite policy trace is missing"
        exit 0
        ;;
esac

exit 0
