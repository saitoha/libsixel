#!/bin/sh
# Policy: docs/quality/metrics.md
# Help names the reference product, size boundary and unavailable output.
set -eux
echo "1..1"
set -v

help=$(${SIXEL_RUNTIME-} "${LSQA_PATH}" --help) || {
    printf '\n%s\n' "not ok 1 - MS-SSIM help"
    exit 0
}
case "${help}" in
    *'MS-SSIM         reference five-scale product (>=176x176)'*'unavailable results print nan; JSON uses null'*) ;;
    *) printf '\n%s\n' "not ok 1 - MS-SSIM help contract"; exit 0 ;;
esac
printf '\n%s\n' "ok 1 - MS-SSIM help contract"
exit 0
