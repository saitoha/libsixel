#!/bin/sh
# Policy: docs/quality/metrics.md
# Help names the reference product, size boundary and compatibility alias.
set -eux
echo "1..1"
set -v

help=$(${SIXEL_RUNTIME-} "${LSQA_PATH}" --help) || {
    echo "not ok 1 - origin help"
    exit 0
}
case "${help}" in
    *'MS-SSIM         reference five-scale product (>=176x176)'*'MS-SSIM-origin  compatibility name for MS-SSIM'*) ;;
    *) echo "not ok 1 - origin help contract"; exit 0 ;;
esac
echo "ok 1 - origin help contract"
exit 0
