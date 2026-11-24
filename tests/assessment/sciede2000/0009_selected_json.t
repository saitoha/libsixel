#!/bin/sh
# Policy: docs/quality/metrics.md
set -eux
echo "1..1"
set -v

value=$(${SIXEL_RUNTIME-} "${LSQA_PATH}" -L 'builtin!' --cms-engine=none -m S-CIELAB,S-CIEDE2000,S-CIEDE2000_max,MS-SSIM "${TOP_SRCDIR}/tests/data/inputs/snake_16.bmp" "${TOP_SRCDIR}/tests/data/inputs/snake_16.bmp") || {
    printf '\n%s\n' "not ok 1 - spatial DE00 selected JSON"
    exit 0
}
case "${value}" in
    *'"color_evaluation"'*|*'"GMSD"'*) printf '\n%s\n' "not ok 1 - unselected JSON field"; exit 0 ;;
esac
case "${value}" in
    *'"S-CIELAB": 0.000000'*'"S-CIEDE2000": 0.000000'*'"S-CIEDE2000_max": 0.000000'*'"MS-SSIM": null'*) ;;
    *) printf '\n%s\n' "not ok 1 - spatial DE00 canonical JSON and unavailable companion"; exit 0 ;;
esac

printf '\n%s\n' "ok 1 - spatial DE00 selected JSON and independent metric availability"
exit 0
