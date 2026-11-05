#!/bin/sh
# Concurrent palette builds access first-use caches through the shared helper.
# Policy: docs/threading/encoder.md
# Policy: docs/testing/staticcheck.md
set -eux
echo '1..1'
set -v
awk -f "$1/tests/_static/awk/palette-option-storage.awk" \
    "$1/src/palette-kmeans.c" "$1/src/palette-kmedoids.c" \
    "$1/src/palette-kcenter.c" || {
    echo 'not ok 1 - quantizer defaults use synchronized cache access'
    exit 0
}
echo 'ok 1 - quantizer defaults use synchronized cache access'
exit 0
