#!/bin/sh
# Verify builtin PSD reports malformed CMYK channel count (<4).

set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n"
    exit 0
}

echo "1..1"
set -v

input_psd="${TOP_SRCDIR}/tests/data/inputs/formats/stbi_minimal_cmyk_channels3_raw.psd"
trace_log=$(set +xv; ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" -L builtin! "${input_psd}" -o /dev/null 2>&1 || true)

case "${trace_log}" in
    *"builtin PSD: malformed channel count (3) for CMYK (requires >=4)"*)
        echo "ok" 1 - "CMYK channel_count<4 is reported as malformed"
        ;;
    *)
        echo "not ok" 1 - "CMYK malformed channel count trace is missing"
        exit 0
        ;;
esac

exit 0
