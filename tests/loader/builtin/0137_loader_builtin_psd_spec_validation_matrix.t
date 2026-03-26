#!/bin/sh
# Verify builtin loader rejects PSD header values outside STB-supported range.

set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n"
    exit 0
}

. "${TOP_SRCDIR}/tests/_lib/sh/common.sh"

echo "1..6"
set -v

test_no=1
for case_entry in \
    "invalid_signature.psd:invalid signature" \
    "wrong_version.psd:unsupported version" \
    "wrong_channel_count.psd:channel count > 16" \
    "unsupported_bit_depth.psd:bit depth other than 8/16" \
    "wrong_color_mode.psd:unsupported color mode (outside Gray/Indexed/RGB/CMYK/Lab)" \
    "bad_compression.psd:compression other than raw/RLE"
do
    file_name=${case_entry%%:*}
    case_desc=${case_entry#*:}
    input_psd="${TOP_SRCDIR}/tests/data/corrupted/${file_name}"

    run_img2sixel -L builtin! "${input_psd}" >/dev/null && {
        echo "not ok" "${test_no}" - "${case_desc} was unexpectedly accepted"
        exit 0
    }

    echo "ok" "${test_no}" - "${case_desc} is rejected"
    test_no=$((test_no + 1))
done

exit 0
