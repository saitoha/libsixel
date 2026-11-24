#!/bin/sh
# Policy: docs/quality/metrics.md
# One MS-SSIM JSON field is null when five scales cannot be measured.
set -eux
echo "1..1"
set -v

value=$(${SIXEL_RUNTIME-} "${LSQA_PATH}" -L 'builtin!' --cms-engine=none \
    "${TOP_SRCDIR}/tests/data/inputs/snake_64.bmp" \
    "${TOP_SRCDIR}/tests/data/inputs/snake_64.bmp") || {
    printf '\n%s\n' "not ok 1 - MS-SSIM JSON output"
    exit 0
}
case "${value}" in
    *'"MS-SSIM": null'*) ;;
    *) printf '\n%s\n' "not ok 1 - unavailable MS-SSIM JSON field"; exit 0 ;;
esac
case "${value}" in
    *'"MS-SSIM-'*) printf '\n%s\n' "not ok 1 - duplicate MS-SSIM JSON field"; exit 0 ;;
esac
printf '\n%s\n' "ok 1 - unavailable MS-SSIM JSON field"
exit 0
