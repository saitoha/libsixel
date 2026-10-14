#!/bin/sh
# Policy: docs/quality/metrics.md
set -eux
echo "1..1"
set -v

help=$(${SIXEL_RUNTIME-} "${LSQA_PATH}" -L 'builtin!' --cms-engine=none --help) || {
    echo "not ok 1 - scielab help"
    exit 0
}
case "${help}" in
    *'--scielab-ppd=PPD: pixels/degree,'*'S-CIELAB and S-CIELAB_max use CIE76.'*) ;;
    *) echo "not ok 1 - scielab help contract"; exit 0 ;;
esac

echo "ok 1 - scielab help describes geometry and scale"
exit 0
