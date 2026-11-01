#!/bin/sh
# Explicit representative selection is rejected for medoids.
# Policy: docs/encoder/palette-construction/quantization/README.md
set -eux
echo "1..1"
set -v
set +e
message=$(${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" -Goff -Qmedoids -scenter \
    "${TOP_SRCDIR}/tests/data/inputs/snake_16.png" 2>&1 >/dev/null)
status=$?
set -e
test "$status" -ne 0 || {
    echo "not ok 1 - unsupported request unexpectedly succeeded"
    exit 0
}
case "$message" in *select-color\ requires\ heckbert*) true ;; *) false ;; esac || {
    echo "not ok 1 - medoids accepted or missed diagnostic"
    exit 0
}
echo "ok 1 - medoids rejects explicit representative"
exit 0
