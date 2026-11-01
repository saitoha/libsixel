#!/bin/sh
# Verify malformed CMYK mft2 ICC payload preserves cms=none sample values.

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

# Hold quantization geometry fixed while testing loader/filter behavior.

input_jpeg="${TOP_SRCDIR}/tests/data/colormgmt/input/custom/cmyk_mft2_truncated.jpg"
output_builtin="${TMPDIR:-/tmp}/libsixel-cmyk-mft2-truncated-builtin-$$.six"
output_none="${TMPDIR:-/tmp}/libsixel-cmyk-mft2-truncated-none-$$.six"

test -f "${input_jpeg}" || {
    echo "not ok" 1 - "missing input fixture: cmyk_mft2_truncated.jpg"
    exit 0
}

${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" -W gamma -Llibjpeg:cms_engine=builtin! "${input_jpeg}" >"${output_builtin}" || {
    echo "not ok" 1 - "libjpeg builtin cms decode failed: cmyk_mft2_truncated"
    exit 0
}

${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" -W gamma -Llibjpeg:cms_engine=none! "${input_jpeg}" >"${output_none}" || {
    echo "not ok" 1 - "libjpeg cms=none decode failed: cmyk_mft2_truncated"
    exit 0
}

# Rejected profile metadata must preserve the unmanaged sample values.
${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" loader/image_values \
    "${output_none}" "${output_builtin}" 'builtin:Enone!' 0 || {
    echo "not ok 1 - rejected ICC fallback changed the unmanaged samples"
    exit 0
}
echo "ok 1 - rejected ICC fallback preserves unmanaged samples"
exit 0
