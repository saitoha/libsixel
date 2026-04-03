#!/bin/sh
# Verify malformed CMYK mft2 ICC payload falls back near cms=none behavior.

set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n"
    exit 0
}

test "${HAVE_JPEG-}" = 1 || {
    printf "1..0 # SKIP libjpeg loader is unavailable\n"
    exit 0
}

echo "1..1"
set -v

lsqa_floor=${LSQA_MS_SSIM_FALLBACK_FLOOR:-0.99}
input_jpeg="${TOP_SRCDIR}/tests/data/colormgmt/input/custom/cmyk_mft2_truncated.jpg"
output_builtin="${TMPDIR:-/tmp}/libsixel-cmyk-mft2-truncated-builtin-$$.six"
output_none="${TMPDIR:-/tmp}/libsixel-cmyk-mft2-truncated-none-$$.six"

test -f "${input_jpeg}" || {
    echo "not ok" 1 - "missing input fixture: cmyk_mft2_truncated.jpg"
    exit 0
}

${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" -Llibjpeg:cms_engine=builtin! "${input_jpeg}" >"${output_builtin}" || {
    echo "not ok" 1 - "libjpeg builtin cms decode failed: cmyk_mft2_truncated"
    exit 0
}

${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" -Llibjpeg:cms_engine=none! "${input_jpeg}" >"${output_none}" || {
    echo "not ok" 1 - "libjpeg cms=none decode failed: cmyk_mft2_truncated"
    exit 0
}

lsqa_msg=$(set +xv; ${SIXEL_RUNTIME-} "${LSQA_PATH}" -m MS-SSIM -b "MS-SSIM:${lsqa_floor}" \
    "${output_none}" "${output_builtin}" 2>&1) || {
    echo "not ok" 1 - "cmyk_mft2_truncated builtin fallback diverged from cms=none (MS-SSIM ${lsqa_floor}): ${lsqa_msg}"
    exit 0
}

echo "ok" 1 - "libjpeg malformed cmyk_mft2 fallback stays near cms=none (MS-SSIM ${lsqa_floor})"
exit 0
