#!/bin/sh
# Policy: docs/quality/metrics.md
# Help distinguishes the legacy mean and reference product and size boundary.
set -eux
echo "1..1"
set -v

help=$(${SIXEL_RUNTIME-} "${LSQA_PATH}" --help) || {
    echo "not ok 1 - origin help"
    exit 0
}
case "${help}" in
    *'MS-SSIM         legacy multiscale arithmetic average'*'MS-SSIM-origin  reference five-scale product (>=176x176)'*) ;;
    *) echo "not ok 1 - origin help contract"; exit 0 ;;
esac
echo "ok 1 - origin help contract"
exit 0
