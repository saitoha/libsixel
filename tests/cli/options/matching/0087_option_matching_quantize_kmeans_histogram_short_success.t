#!/bin/sh
# TAP test verifying -Q accepts short-form kmeans histogram suboptions.
# Policy: docs/cli/design-policy.md

set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n";
    exit 0
}


echo "1..1"
set -v

${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
    -Qauto:binning_policy=soft -Qk:N6:Msrgb:Dtrilinear:F1 \
    "${TOP_SRCDIR}/tests/data/inputs/small.ppm" \
    >/dev/null || {
    echo "not ok" 1 - "-Q kmeans short histogram suboptions were rejected"
    exit 0
}

echo "ok" 1 - "-Q accepts short kmeans histogram suboptions"
exit 0
