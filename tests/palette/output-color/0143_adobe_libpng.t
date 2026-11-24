#!/bin/sh
# Policy: docs/encoder/output/output-colorspace.md
set -eux

echo "1..1"
set -v

{ test "${HAVE_IMG2SIXEL-}" = 1 && test "${HAVE_LIBPNG-}" = 1; } || {
    printf '\n%s\n' "ok 1 # SKIP img2sixel or libpng disabled"
    exit 0
}
${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
    -L'libpng:cms_engine=builtin:cms_target=linear:prefer_8bit=0!' \
    --threading-policy=greedy:global_budget=1 -.auto -Qheckbert -Fnone -aoff -_none '-~none' -dnone \
    -Xlinear -Wlinear -Uadobe-rgb -trgb -p2 \
    "${TOP_SRCDIR}/tests/data/color-numeric/named-output-p3.png" | \
    ${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" quant/palette/named_output/0037_adobe_libpng || {
    printf '\n%s\n' "not ok 1 - P3 libpng input to adobe-rgb"
    exit 0
}
printf '\n%s\n' "ok 1 - P3 libpng input to adobe-rgb"
exit 0
