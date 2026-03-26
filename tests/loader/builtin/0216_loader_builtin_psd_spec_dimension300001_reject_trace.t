#!/bin/sh
# Verify PSD dimension boundary rejects 300001.

set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n"
    exit 0
}

. "${TOP_SRCDIR}/tests/_lib/sh/common.sh"

echo "1..1"
set -v

input_psd="${TOP_SRCDIR}/tests/data/inputs/formats/stbi_minimal_dimension300001_rgb.psd"

trace_log=$(set +xv; run_img2sixel -L builtin! "${input_psd}" -o /dev/null 2>&1 || true)

case "${trace_log}" in
    *"builtin PSD: malformed dimensions (300001x1; expected"*)
        echo "ok" 1 - "dimension=300001 is rejected by boundary validation"
        ;;
    *)
        echo "not ok" 1 - "dimension=300001 rejection trace is missing"
        exit 0
        ;;
esac

exit 0
