#!/bin/sh
# Test-plan: docs/testing/builtin-loader-coverage.md
# Policy: docs/loader/builtin/psd.md
# Preserve permissive acceptance of the historical incomplete CMYK ICC.
set -eux
echo "1..1"
set -v

test "${HAVE_IMG2SIXEL-}" = 1 || {
    echo "ok 1 - incomplete CMYK ICC acceptance # SKIP img2sixel unavailable"
    exit 0
}
trace=$(SIXEL_TRACE_TOPIC=loader ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
    -Lbuiltin:cms_engine=builtin! \
    "${TOP_SRCDIR}/tests/data/inputs/formats/stbi_minimal_mode7_cmyk8_mft2_valid_icc_profile.psd" \
    2>&1 >/dev/null) || {
    echo "not ok 1 - historical incomplete CMYK ICC rejected"
    exit 0
}
test "${trace#*"event=apply|engine=builtin|path=extended-rgb|src=5|dst=4|ok=1"}" != "$trace" || {
    echo "not ok 1 - historical CMYK LUT was not actually applied"
    exit 0
}
echo "ok 1 - historical incomplete CMYK ICC accepted and applied"
exit 0
