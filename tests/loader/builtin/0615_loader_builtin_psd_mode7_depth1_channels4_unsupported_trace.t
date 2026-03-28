#!/bin/sh
# Verify Multichannel(mode=7) channels=4 with depth=1 is explicitly unsupported.
# Fixture generation command:
#   python3 tests/data/inputs/formats/generate_psd_policy_trace_fixtures.py

set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n"
    exit 0
}


echo "1..1"
set -v

input_psd="${TOP_SRCDIR}/tests/data/inputs/formats/stbi_minimal_mode7_depth1_channels4_raw.psd"
trace_log=$(set +xv; ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" -L builtin! "${input_psd}" -o /dev/null 2>&1 || true)

case "${trace_log}" in
    *"builtin PSD: unsupported bit depth (1) for Multichannel (4ch->CMYK)"*)
        echo "ok" 1 - "Multichannel channels=4 depth=1 is rejected by bit-depth policy"
        ;;
    *)
        echo "not ok" 1 - "Multichannel channels=4 depth=1 did not emit expected unsupported trace"
        ;;
esac

exit 0
