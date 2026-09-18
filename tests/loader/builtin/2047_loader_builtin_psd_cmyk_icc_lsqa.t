#!/bin/sh
# Test-plan: docs/testing/builtin-loader-coverage.md
# Policy: docs/loader/builtin/psd.md
# Compare normal CMYK ICC encoding with an independently generated PNG.
set -eux
echo "1..1"
set -v

test "${HAVE_IMG2SIXEL-}" = 1 || {
    echo "ok 1 - independent PSD CMYK quality # SKIP img2sixel unavailable"
    exit 0
}
test -d "${ARTIFACT_LOCAL_DIR}" || mkdir -p "${ARTIFACT_LOCAL_DIR}"
output="${ARTIFACT_LOCAL_DIR}/independent-cmyk.six"
trace=$(SIXEL_TRACE_TOPIC=loader \
    SIXEL_LOADER_CMS_RENDERING_INTENT=relative! ${SIXEL_RUNTIME-} \
    "${IMG2SIXEL_PATH}" -Lbuiltin:cms_engine=builtin! \
    "${TOP_SRCDIR}/tests/data/color-numeric/cmyk.psd" \
    2>&1 >"$output") || {
    echo "not ok 1 - independent PSD CMYK conversion failed"
    exit 0
}
case "${trace}" in *"event=apply|engine=builtin|path=extended-rgb|src=5|dst=4|ok=1"*) true ;; *) false ;; esac || {
    echo "not ok 1 - normal CMYK ICC was not actually applied by builtin"
    exit 0
}
${SIXEL_RUNTIME-} "${LSQA_PATH}" --cms-engine none -m MS-SSIM \
    -b MS-SSIM:0.98 \
    "${TOP_SRCDIR}/tests/data/color-numeric/cmyk-reference.png" \
    "$output" >/dev/null || {
    echo "not ok 1 - independent PSD CMYK quality below 0.98"
    exit 0
}
echo "ok 1 - independent PSD CMYK quality meets 0.98 with builtin CMS"
exit 0
