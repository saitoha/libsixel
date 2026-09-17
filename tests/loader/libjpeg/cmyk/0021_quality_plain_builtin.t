#!/bin/sh
# Policy: docs/loader/libjpeg.md
# Compare final SIXEL with the independent affine CMYK image.
set -eux
test "${HAVE_IMG2SIXEL-}" = 1 && test "${HAVE_JPEG-}" = 1 || {
    echo "1..0 # SKIP img2sixel or libjpeg is unavailable"
    exit 0
}
result=0
${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    jpeg-cmyk/0002_affine_plain_builtin "${TOP_SRCDIR}" 1>&2 || result=$?
test "$result" -ne 77 || {
    echo "1..0 # SKIP required CMS engine is unavailable"
    set -v
    exit 0
}
echo "1..1"
set -v
test "$result" -eq 0 || {
    echo "not ok 1 - numeric route qualification failed"
    exit 0
}
test -d "${ARTIFACT_LOCAL_DIR}" || mkdir -p "${ARTIFACT_LOCAL_DIR}"
${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" --threads=1 \
    -L 'libjpeg:cms_engine=builtin:cms_intent=relative!!' \
    "${TOP_SRCDIR}/tests/data/color-numeric/jpeg-cmyk-affine-plain.jpg" \
    >"${ARTIFACT_LOCAL_DIR}/output.six" || {
    echo "not ok 1 - JPEG conversion failed"
    exit 0
}
${SIXEL_RUNTIME-} "${LSQA_PATH}" -m MS-SSIM -b MS-SSIM:0.98 \
    "${TOP_SRCDIR}/tests/data/color-numeric/jpeg-cmyk-plain-reference.png" \
    "${ARTIFACT_LOCAL_DIR}/output.six" 1>&2 || {
    echo "not ok 1 - independent CMYK MS-SSIM below 0.98"
    exit 0
}
echo "ok 1 - independent CMYK quality plain builtin"
exit 0
