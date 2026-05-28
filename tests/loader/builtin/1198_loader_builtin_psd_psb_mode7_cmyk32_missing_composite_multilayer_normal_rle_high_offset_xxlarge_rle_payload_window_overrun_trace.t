#!/bin/sh
# Verify PSB mode7(4ch->CMYK)32 xxlarge high-offset RLE payload-window overrun emits dedicated malformed trace.
# Fixture is prepared under TOP_BUILDDIR/tests/data/inputs/formats
# by tests/_static/sh/prepare-psb-large-fixtures.sh

set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n"
    exit 0
}

test -n "${SIXEL_TEST_GZIP-}" || {
    printf "1..0 # SKIP gzip is unavailable for compressed PSB fixture\n"
    exit 0
}

echo "1..1"
set -v

input_psd="${TOP_BUILDDIR}/tests/data/inputs/formats/snake16_psb_mode7_cmyk32_missing_composite_multilayer_normal_rle_high_offset_xxlarge_rle_payload_window_overrun.psd"
trace_log=$(set +xv; ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" -L builtin! "${input_psd}" -o /dev/null 2>&1 || true)

case "${trace_log}" in
    *"builtin PSD: malformed PSB RLE row length"*)
        echo "ok" 1 - "PSB mode7(4ch->CMYK)32 xxlarge high-offset RLE payload-window overrun keeps dedicated malformed trace"
        ;;
    *)
        echo "not ok" 1 - "PSB mode7(4ch->CMYK)32 xxlarge high-offset RLE payload-window overrun trace is missing"
        exit 0
        ;;
esac

exit 0
