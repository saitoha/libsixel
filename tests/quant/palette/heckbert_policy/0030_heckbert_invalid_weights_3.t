#!/bin/sh
# Reject invalid ratio `1/1` with a diagnostic.
# Policy: docs/encoder/palette-construction/quantization/README.md
set -eux
echo "1..1"
set -v
set +e
message=$(${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
    -Qheckbert:weights=1/1 \
    "${TOP_SRCDIR}/tests/data/inputs/snake_16.png" 2>&1 >/dev/null)
status=$?
set -e
test "$status" -ne 0 || {
    echo "not ok 1 - unsupported request unexpectedly succeeded"
    exit 0
}
case "$message" in *weights*) true ;; *) false ;; esac || {
    echo "not ok 1 - invalid ratio accepted or undiagnosed"
    exit 0
}
echo "ok 1 - invalid_weights_3"
exit 0
