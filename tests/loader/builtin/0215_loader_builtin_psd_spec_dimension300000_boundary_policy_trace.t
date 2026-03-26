#!/bin/sh
# Verify PSD dimension boundary 300000 is accepted before policy checks.

set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n"
    exit 0
}

. "${TOP_SRCDIR}/tests/_lib/sh/common.sh"

echo "1..1"
set -v

input_psd="${TOP_SRCDIR}/tests/data/inputs/formats/stbi_minimal_dimension300000_multichannel.psd"

trace_log=$(set +xv; run_img2sixel -L builtin! "${input_psd}" -o /dev/null 2>&1 || true)

case "${trace_log}" in
    *"builtin PSD: unsupported color mode (7: Multichannel)"*)
        echo "ok" 1 - "dimension=300000 passes boundary and reaches policy"
        ;;
    *)
        echo "not ok" 1 - "dimension=300000 did not pass boundary validation"
        exit 0
        ;;
esac

exit 0
