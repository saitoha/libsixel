#!/bin/sh
# Policy: docs/loader/color-management/README.md
# Verify actual lcms2 execution and independent final RGB through the CLI.
set -eux
echo "1..1"
set -v

test "${HAVE_IMG2SIXEL-}" = 1 || {
    echo "ok 1 - lcms2 CLI colors # SKIP img2sixel unavailable"
    exit 0
}
test "${HAVE_LCMS2-}" = 1 || {
    echo "ok 1 - lcms2 CLI colors # SKIP lcms2 not compiled"
    exit 0
}
test -d "${ARTIFACT_LOCAL_DIR}" || mkdir -p "${ARTIFACT_LOCAL_DIR}"
output="${ARTIFACT_LOCAL_DIR}/lcms2-independent-rgb.six"
trace=$(SIXEL_TRACE_TOPIC=loader ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
    --cms-engine=lcms2 -Lbuiltin! \
    "${TOP_SRCDIR}/tests/data/color-numeric/cli-rgb.jpg" \
    2>&1 >"$output") || {
    echo "not ok 1 - lcms2 CLI conversion failed"
    exit 0
}
record=${trace##*"LSXCMS2|event=apply|"}
test "${record%%[![:print:]]*}" = \
    "engine=lcms2|path=external|src=4|dst=4|ok=1" || {
    echo "not ok 1 - independent JPEG profile was not applied by lcms2"
    exit 0
}
${SIXEL_RUNTIME-} "${LSQA_PATH}" --cms-engine=none -m MS-SSIM \
    -b MS-SSIM:0.98 \
    "${TOP_SRCDIR}/tests/data/color-numeric/cli-rgb-reference.png" \
    "$output" >/dev/null || {
    echo "not ok 1 - lcms2 CLI independent color quality below 0.98"
    exit 0
}
echo "ok 1 - lcms2 CLI applies and preserves independent RGB colors"
exit 0
