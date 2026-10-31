#!/bin/sh
# Policy: docs/quality/metrics.md
# Policy: docs/quality/video-evaluation.md
set -eux
echo "1..1"
set -v

# Video replay converts paint through XYZ and linear RGB float32, while the
# reference retains byte samples. Allow the resulting sub-1e-5 color error.
value=$(${SIXEL_RUNTIME-} "${LSQA_PATH}" -L 'builtin!' --cms-engine=none -Ma --reference-fps=1 -m S-CIEDE2000_max -b S-CIEDE2000_max:max=0.00001 "${TOP_SRCDIR}/tests/data/inputs/snake_64.six" "${TOP_SRCDIR}/tests/data/inputs/snake_64.six") || {
    echo "not ok 1 - spatial DE00 video maximum"
    exit 0
}
case "${value}" in
    0.00000[0-9]) ;;
    *) echo "not ok 1 - spatial DE00 video maximum value"; exit 0 ;;
esac

echo "ok 1 - spatial DE00 high-bit video selection and gate"
exit 0
