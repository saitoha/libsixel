#!/bin/sh
# Policy: docs/quality/metrics.md
set -eux
echo "1..1"
set -v

value=$(${SIXEL_RUNTIME-} "${LSQA_PATH}" -L 'builtin!' --cms-engine=none --ppd=224 -v28.5 "${TOP_SRCDIR}/tests/data/inputs/snake_16.bmp" "${TOP_SRCDIR}/tests/data/inputs/snake_16.bmp") || {
    echo "not ok 1 - spatial DE00 metadata"
    exit 0
}
case "${value}" in
    *'"ppd": 28.5'*'"scielab_difference": "CIE76"'*'"sciede2000_difference": "CIEDE2000"'*'"sciede2000_parameters": {"kL": 1, "kC": 1, "kH": 1}'*'"S-CIEDE2000": 0.000000'*'"S-CIEDE2000_max": 0.000000'*) ;;
    *) echo "not ok 1 - shared geometry and distinct difference metadata"; exit 0 ;;
esac

echo "ok 1 - spatial DE00 full JSON and last shared PPD"
exit 0
