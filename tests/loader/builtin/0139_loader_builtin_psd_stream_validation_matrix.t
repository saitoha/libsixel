#!/bin/sh
# Verify builtin loader rejects PSD stream-level corruption and overflow headers.

set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n"
    exit 0
}

. "${TOP_SRCDIR}/tests/_lib/sh/common.sh"

echo "1..4"
set -v

test_no=1
for case_entry in \
    "invalid_rle_stream.psd:invalid RLE stream" \
    "too_large_dimension.psd:dimension larger than STB max" \
    "bad_color_mode_data_length.psd:color mode data section length overflow" \
    "bad_image_resources_length.psd:image resources section length overflow"
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
