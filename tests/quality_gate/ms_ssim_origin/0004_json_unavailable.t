#!/bin/sh
# Policy: docs/quality/metrics.md
# Both JSON keys are null when five scales cannot be measured.
set -eux
echo "1..1"
set -v

value=$(${SIXEL_RUNTIME-} "${LSQA_PATH}" -L 'builtin!' --cms-engine=none \
    "${TOP_SRCDIR}/tests/data/inputs/snake_64.bmp" \
    "${TOP_SRCDIR}/tests/data/inputs/snake_64.bmp") || {
    echo "not ok 1 - origin JSON output"
    exit 0
}
case "${value}" in
    *'"MS-SSIM": null'*'"MS-SSIM-origin": null'*) ;;
    *) echo "not ok 1 - unavailable origin JSON field"; exit 0 ;;
esac
echo "ok 1 - unavailable origin JSON field"
exit 0
