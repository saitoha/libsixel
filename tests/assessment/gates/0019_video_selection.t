#!/bin/sh
# Policy: docs/quality/metrics.md
# Video reports named means and checks combined gates
set -eux
echo "1..1"
set -v

status=0
value=$(${SIXEL_RUNTIME-} "${LSQA_PATH}" -L 'builtin!' --cms-engine=none -Ma -m PSNR_Y,GMSD -b PSNR_Y:min=0,GMSD:max=1 "${TOP_SRCDIR}/tests/data/assessment/video-snake-16.png" "${TOP_SRCDIR}/tests/data/assessment/video-snake-16.six") || status=$?

test "${status}" -eq 0 || {
    printf '\n%s\n' "not ok 1 - unexpected exit status"
    exit 0
}
case "${value}" in
    '{
  "GMSD": 0.0082'*',
  "PSNR_Y": 42.37'*'
}') true ;;
    *) false ;;
esac || {
    printf '\n%s\n' "not ok 1 - Video reports named means and checks combined gates"
    exit 0
}
printf '\n%s\n' "ok 1 - Video reports named means and checks combined gates"
exit 0
