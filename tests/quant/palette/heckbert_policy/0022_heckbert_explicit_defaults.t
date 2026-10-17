#!/bin/sh
# Explicit partition defaults preserve output.
# Policy: docs/encoder/palette-construction/quantization/README.md
set -eux
echo "1..1"
set -v
implicit=$(${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" -Goff -p16 \
    "${TOP_SRCDIR}/tests/data/inputs/snake_16.png")
explicit=$(${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" -Goff -p16 \
    -Qheckbert:cut=median:order=population:weights=auto \
    "${TOP_SRCDIR}/tests/data/inputs/snake_16.png")
test -n "$implicit" && test "$implicit" = "$explicit" || {
    echo "not ok 1 - explicit defaults changed output"
    exit 0
}
echo "ok 1 - explicit_defaults"
exit 0
