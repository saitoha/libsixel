#!/bin/sh
# Verify builtin PSD decodes PSB (version=2) minimal RLE composite input.

set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n"
    exit 0
}

echo "1..1"
set -v

input_psd="${TOP_SRCDIR}/tests/data/inputs/formats/stbi_minimal_version2_rgb_rle.psd"
trace_log=$(set +xv; ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" -L builtin! "${input_psd}" -o /dev/null 2>&1) || {
    echo "not ok" 1 - "version=2 PSB RLE failed to decode"
    exit 0
}

case "${trace_log}" in
    *"builtin PSD: unsupported version"*)
        echo "not ok" 1 - "version=2 PSB RLE was rejected as unsupported"
        exit 0
        ;;
    *"builtin PSD: malformed section length/offset"*)
        echo "not ok" 1 - "version=2 PSB RLE section parsing failed"
        exit 0
        ;;
    *)
        echo "ok" 1 - "version=2 PSB RLE decodes without unsupported-version trace"
        ;;
esac

exit 0
