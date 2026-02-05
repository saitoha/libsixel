#!/bin/sh
# Expect truncated PNG input to be rejected or graded as low quality.

set -eu

if [ "${VERBOSE:-0}" -eq 1 ]; then
    set -x
fi



lsqa_common_path="${TOP_SRCDIR}/tests/lib/sh/lsqa/lsqa_common.sh"
LSQA_HELPER_DIR=$(CDPATH=; cd "$(dirname "${lsqa_common_path}")" && pwd)
export LSQA_HELPER_DIR
. "${lsqa_common_path}"


status=0



printf '1..1\n'
set -v

image_path="${top_srcdir}/tests/data/corrupted/truncated.png"
if lsqa_expect_low_quality_or_fail "${image_path}" \
    "truncated.png" "${ARTIFACT_LOCAL_DIR}"; then
    pass 1 "truncated input rejected or scored low"
else
    fail 1 "truncated input unexpectedly accepted"
fi

exit "${status}"
