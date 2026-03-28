#!/bin/sh
# Verify malformed layer-only Gray8 PSD is deterministically rejected by
# missing-composite fallback layout policy.

set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n"
    exit 0
}

echo "1..1"
set -v

input_psd="${TOP_SRCDIR}/tests/data/inputs/formats/stbi_minimal_missing_composite_gray.psd"
trace_log=$(set +xv; ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" -L builtin! "${input_psd}" -o /dev/null 2>&1 || true)

case "${trace_log}" in
    *"builtin PSD: unsupported layer fallback layout"*)
        echo "ok" 1 - "malformed layer-only Gray8 PSD is rejected by fallback layout policy"
        ;;
    *)
        echo "not ok" 1 - "gray layer-only fallback rejection trace is missing"
        exit 0
        ;;
esac

exit 0
