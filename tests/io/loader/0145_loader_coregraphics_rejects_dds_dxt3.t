#!/bin/sh
# TAP test: CoreGraphics decodes DDS DXT3 input when macOS supports DDS UTI.
# Apple Uniform Type Identifiers documentation reports DDS UTI support from
# macOS 14.0, so older macOS releases are skipped in this test.

set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n"
    exit 0
}

. "${TOP_SRCDIR}/tests/_lib/sh/common.sh"

test "${HAVE_COREGRAPHICS-}" = 1 || {
    printf "1..0 # SKIP coregraphics support is disabled in this build\n"
    exit 0
}

macos_version="${SIXEL_TEST_MACOS_PRODUCT_VERSION-}"
dds_min_macos="13.0"

test -n "${macos_version}" || {
    printf "1..0 # SKIP macOS version is unavailable\n"
    exit 0
}

macos_major="${macos_version%%.*}"
macos_rest="${macos_version#*.}"
test "${macos_rest}" = "${macos_version}" && macos_rest=0
macos_minor="${macos_rest%%.*}"

dds_min_major="${dds_min_macos%%.*}"
dds_min_minor="${dds_min_macos#*.}"

dds_supported=0
test "${macos_major}" -gt "${dds_min_major}" && dds_supported=1
test "${macos_major}" -eq "${dds_min_major}" && \
    test "${macos_minor}" -ge "${dds_min_minor}" && dds_supported=1

test "${dds_supported}" -eq 1 || {
    printf "1..0 # SKIP DDS is unsupported on macOS %s (< %s)\n" \
        "${macos_version}" "${dds_min_macos}"
    exit 0
}

echo "1..1"
set -v

lsqa_floor=0.95
image_path="${TOP_SRCDIR}/tests/data/inputs/formats/snake-dds-dxt3.dds"
reference_path="${TOP_SRCDIR}/tests/data/inputs/formats/snake-64-reference-rgb.png"
output_sixel="${ARTIFACT_LOCAL_DIR}/coregraphics_dds_dxt3.six"

run_img2sixel -L coregraphics! "${image_path}" >"${output_sixel}" || {
    fail 1 "coregraphics failed to decode DDS DXT3 input"
    exit 0
}

lsqa_err=$(
    set +xv
    run_lsqa -b "MS-SSIM:${lsqa_floor}" "${reference_path}" "${output_sixel}" 2>&1
) || lsqa_run_status=$?

test "${lsqa_run_status:-0}" -eq 0 && {
    pass 1 "coregraphics decodes DDS DXT3 with expected quality"
    exit 0
}

test "${lsqa_run_status}" -eq 5 && {
    fail 1 "${lsqa_err}"
    exit 0
}

fail 1 "coregraphics DDS DXT3 quality regressed"
exit 0
