#!/bin/sh
# Policy: docs/loader/libpng.md
# Verify libpng direct gAMA 33333 rounds only at the final byte target.
set -eux
echo "1..1"
set -v

result=0
${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    loader/0186_png_byte_target libpng 1 \
    "${TOP_SRCDIR}/tests/data/color-numeric/png-byte-target-1.png" \
    || result=$?
test "$result" -ne 77 || {
    echo "ok 1 # SKIP direct gAMA contract or requested loader is unavailable"
    exit 0
}
test "$result" -eq 0 || {
    echo "not ok 1 - libpng gAMA 33333 final byte target"
    exit 0
}
echo "ok 1 - libpng gAMA 33333 rounds only at the byte target"
exit 0
