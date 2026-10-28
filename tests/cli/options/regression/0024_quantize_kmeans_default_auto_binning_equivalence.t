#!/bin/sh
# Policy: docs/encoder/palette-construction/pipeline/README.md
# Default and explicit AUTO share the sample-adaptive hard-grid policy.
# Top-level option regression
# Image assertion: option-values-and-output-equivalence
set -eux

SIXEL_PALETTE_KMEANS_SEED=1
export SIXEL_PALETTE_KMEANS_SEED
test "${HAVE_IMG2SIXEL-}" = 1 || {
    echo "1..0 # SKIP img2sixel is disabled in this build"
    exit 0
}
echo "1..1"
set -v

# SIXEL is a text stream without NUL bytes; retain both results in memory.
default_output=$(${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" -p 16 -Qkmeans \
    "${TOP_SRCDIR}/tests/data/inputs/snake_16.png") || {
    echo "not ok 1 - default Kmeans binning conversion failed"
    exit 0
}
auto_output=$(${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
    --binning-policy=auto -Qauto -p 16 -Qkmeans \
    "${TOP_SRCDIR}/tests/data/inputs/snake_16.png") || {
    echo "not ok 1 - explicit AUTO Kmeans binning conversion failed"
    exit 0
}
test "$default_output" = "$auto_output" || {
    echo "not ok 1 - default and AUTO Kmeans binning output differ"
    exit 0
}
test -n "$default_output" || {
    echo "not ok 1 - option-route output is empty"
    exit 0
}
echo "ok 1 - default Kmeans binning retains explicit AUTO output"
exit 0
