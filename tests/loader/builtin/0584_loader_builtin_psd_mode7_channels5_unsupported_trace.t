#!/bin/sh
# Verify Multichannel(mode=7) with channels!=3/4 is explicitly unsupported.
# Fixture generation command:
#   python3 tests/data/inputs/formats/generate_psd_policy_trace_fixtures.py

set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n"
    exit 0
}


echo "1..1"
set -v

input_psd="${TOP_SRCDIR}/tests/data/inputs/formats/stbi_minimal_mode7_channels5_raw.psd"
trace_log=$(set +xv; ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" -L builtin! "${input_psd}" -o /dev/null 2>&1 || true)

case "${trace_log}" in
    *"builtin PSD: unsupported Multichannel channel count (5; expected 3 or 4)"*)
        echo "ok" 1 - "Multichannel channels=5 is rejected by channel-count policy"
        ;;
    *)
        echo "not ok" 1 - "Multichannel channels=5 did not emit expected unsupported trace"
        ;;
esac

exit 0
