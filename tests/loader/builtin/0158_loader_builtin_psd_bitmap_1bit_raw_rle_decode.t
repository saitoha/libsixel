#!/bin/sh
# Verify builtin loader decodes Bitmap 1-bit PSD (raw and RLE).

set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n"
    exit 0
}

. "${TOP_SRCDIR}/tests/_lib/sh/common.sh"

echo "1..2"
set -v

test_no=1
for case_entry in \
    "stbi_minimal_bitmap1.psd:bitmap 1-bit raw" \
    "stbi_minimal_bitmap1_rle.psd:bitmap 1-bit RLE"
do
    file_name=${case_entry%%:*}
    case_desc=${case_entry#*:}
    input_psd="${TOP_SRCDIR}/tests/data/inputs/formats/${file_name}"

    run_img2sixel -L builtin! "${input_psd}" >/dev/null || {
        echo "not ok" "${test_no}" - "builtin loader failed to decode ${case_desc}"
        exit 0
    }

    echo "ok" "${test_no}" - "builtin loader decodes ${case_desc}"
    test_no=$((test_no + 1))
done

exit 0
