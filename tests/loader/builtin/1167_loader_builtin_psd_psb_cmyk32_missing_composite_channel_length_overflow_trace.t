#!/bin/sh
# Verify PSB mode4 CMYK32 missing-composite fixture with channel-length overflow is diagnosed.
# Fixture generation commands:
#   python3 tests/data/inputs/formats/generate_psb_missing_composite_fixtures.py

set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n"
    exit 0
}

echo "1..1"
set -v

input_psd="${TOP_SRCDIR}/tests/data/inputs/formats/snake16_psb_cmyk32_missing_composite_multilayer_channel_length_overflow.psd"
trace_log=$(set +xv; ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" -L builtin! "${input_psd}" -o /dev/null 2>&1 || true)

case "${trace_log}" in
    *"builtin PSD: malformed PSB layer channel length"*)
        echo "ok" 1 - "PSB mode4 CMYK32 channel-length overflow emits dedicated malformed trace"
        ;;
    *)
        echo "not ok" 1 - "PSB mode4 CMYK32 channel-length overflow trace is missing"
        exit 0
        ;;
esac

exit 0
