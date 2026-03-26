#!/bin/sh
# Verify builtin loader decodes non-RGB PSD ZIP variants.

set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n"
    exit 0
}

. "${TOP_SRCDIR}/tests/_lib/sh/common.sh"

echo "1..15"
set -v

test_no=1
for case_entry in \
    "stbi_minimal_gray8_zip.psd:Gray 8-bit ZIP" \
    "stbi_minimal_gray8_zip_pred.psd:Gray 8-bit ZIP+prediction" \
    "stbi_minimal_indexed8_zip.psd:Indexed 8-bit ZIP" \
    "stbi_minimal_indexed8_zip_pred.psd:Indexed 8-bit ZIP+prediction" \
    "stbi_minimal_cmyk8_zip.psd:CMYK 8-bit ZIP" \
    "stbi_minimal_cmyk8_zip_pred.psd:CMYK 8-bit ZIP+prediction" \
    "stbi_minimal_lab8_zip.psd:Lab 8-bit ZIP" \
    "stbi_minimal_lab8_zip_pred.psd:Lab 8-bit ZIP+prediction" \
    "stbi_minimal_duotone8_zip.psd:Duotone 8-bit ZIP" \
    "stbi_minimal_duotone8_zip_pred.psd:Duotone 8-bit ZIP+prediction" \
    "stbi_minimal_gray16_zip.psd:Gray 16-bit ZIP" \
    "stbi_minimal_gray16_zip_pred.psd:Gray 16-bit ZIP+prediction" \
    "stbi_minimal_duotone16_zip.psd:Duotone 16-bit ZIP" \
    "stbi_minimal_duotone16_zip_pred.psd:Duotone 16-bit ZIP+prediction" \
    "stbi_minimal_bitmap1_zip.psd:Bitmap 1-bit ZIP"
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
