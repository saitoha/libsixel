#!/bin/sh
# Verify PSB mode4 CMYK32 xxxxxxxlarge over-limit input fails with deterministic allocation error trace.
# Fixture generation commands:
#   python3 tests/data/inputs/formats/generate_psb_missing_composite_fixtures.py

set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n"
    exit 0
}

echo "1..1"
set -v

input_psd="${TOP_SRCDIR}/tests/data/inputs/formats/snake16_psb_cmyk32_missing_composite_multilayer_normal_high_offset_xxxxxxxlarge_over_limit.psd"
trace_log=
trace_log=$(set +xv; ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" -L builtin! "${input_psd}" -o /dev/null 2>&1 || true)

case "${trace_log}" in
    *"sixel_chunk_from_file: input exceeds allocation limit."*|*"bad allocation error"*)
        echo "ok" 1 - "PSB mode4 CMYK32 xxxxxxxlarge over-limit keeps deterministic allocation failure trace"
        ;;
    *)
        echo "not ok" 1 - "PSB mode4 CMYK32 xxxxxxxlarge over-limit allocation failure trace is missing"
        exit 0
        ;;
esac

exit 0
