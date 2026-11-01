#!/bin/sh
# Policy: docs/loader/color-management/README.md
# Verify actual colorsync execution and independent final RGB through the CLI.
set -eux
echo "1..1"
set -v

test "${HAVE_IMG2SIXEL-}" = 1 || {
    echo "ok 1 - colorsync CLI colors # SKIP img2sixel unavailable"
    exit 0
}
test "${HAVE_COREGRAPHICS-}" = 1 || {
    echo "ok 1 - colorsync CLI colors # SKIP colorsync not compiled"
    exit 0
}
test -d "${ARTIFACT_LOCAL_DIR}" || mkdir -p "${ARTIFACT_LOCAL_DIR}"
output="${ARTIFACT_LOCAL_DIR}/colorsync-independent-rgb.six"
trace=$(SIXEL_TRACE_TOPIC=loader ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" --lookup-policy=none \
    --cms-engine=colorsync -Lbuiltin! \
    "${TOP_SRCDIR}/tests/data/color-numeric/cli-rgb.jpg" \
    2>&1 >"$output") || {
    echo "not ok 1 - colorsync CLI conversion failed"
    exit 0
}
record=${trace##*"LSXCMS2|event=apply|"}
test "${record%%[![:print:]]*}" = \
    "engine=colorsync|path=external|src=4|dst=4|ok=1" || {
    echo "not ok 1 - independent JPEG profile was not applied by colorsync"
    exit 0
}
# Eight exact source colors require no palette reduction. Full scanning
# avoids lookup-table approximation. The sample budget is 1.275 levels for
# SIXEL percentage rounding, 0.5 for decoding, 0.5 for the PNG oracle, and
# 0.025 for floating-point color arithmetic (2.3 RGB byte levels total).
${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" loader/image_values \
    \
    "${TOP_SRCDIR}/tests/data/color-numeric/cli-rgb-reference.png" \
    "$output" 'builtin:Enone!' 2.3 || {
    echo "not ok 1 - colorsync CLI independent RGB samples exceed 2.3 levels"
    exit 0
}
echo "ok 1 - colorsync CLI applies and preserves independent RGB colors"
exit 0
