#!/bin/sh
# Verify PSD ZIP stream truncation is malformed even when layer records exist.

set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n"
    exit 0
}


echo "1..1"
set -v

input_psd="${TOP_SRCDIR}/tests/data/inputs/formats/stbi_minimal_missing_composite_rgb_zip_truncated.psd"

trace_log=$(set +xv; ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" -L builtin! "${input_psd}" -o /dev/null 2>&1 || true)

case "${trace_log}" in
    *"builtin PSD: malformed compressed channel stream"*)
        case "${trace_log}" in
            *"builtin PSD: unsupported file without merged/composite image"*)
                echo "not ok" 1 - "ZIP truncation must not be treated as missing composite"
                exit 0
                ;;
            *)
                echo "ok" 1 - "ZIP truncation with layer records is classified as malformed"
                ;;
        esac
        ;;
    *)
        echo "not ok" 1 - "ZIP truncation malformed trace is missing"
        exit 0
        ;;
esac

exit 0
