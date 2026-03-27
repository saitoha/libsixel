#!/bin/sh
# Verify PSD channel upper boundary (56) reaches policy checks.

set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n"
    exit 0
}


echo "1..1"
set -v

input_psd="${TOP_SRCDIR}/tests/data/inputs/formats/stbi_minimal_channels56_multichannel.psd"

trace_log=$(set +xv; ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" -L builtin! "${input_psd}" -o /dev/null 2>&1 || true)

case "${trace_log}" in
    *"builtin PSD: unsupported color mode (7: Multichannel)"*)
        echo "ok" 1 - "channel=56 passes boundary and hits multichannel policy"
        ;;
    *)
        echo "not ok" 1 - "channel=56 did not reach multichannel policy check"
        exit 0
        ;;
esac

exit 0
