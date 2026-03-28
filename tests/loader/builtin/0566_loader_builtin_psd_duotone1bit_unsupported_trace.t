#!/bin/sh
# Verify builtin PSD rejects Duotone depth=1 with deterministic trace.

set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n"
    exit 0
}

echo "1..1"
set -v

input_psd="${TOP_SRCDIR}/tests/data/inputs/formats/stbi_minimal_duotone1bit_raw.psd"
trace_log=$(set +xv; ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" -L builtin! "${input_psd}" -o /dev/null 2>&1 || true)

case "${trace_log}" in
    *"builtin PSD: unsupported bit depth (1) for Duotone"*)
        echo "ok" 1 - "Duotone depth=1 is rejected as unsupported"
        ;;
    *)
        echo "not ok" 1 - "Duotone unsupported bit depth trace is missing"
        exit 0
        ;;
esac

exit 0
