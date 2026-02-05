#!/bin/sh
# Expect noisy JPEG headers to be rejected or graded as low quality.

set -eu

if [ "${VERBOSE:-0}" -eq 1 ]; then
    set -x
fi

lsqa_common_path="${TOP_SRCDIR}/tests/lib/sh/lsqa/lsqa_common.sh"
. "${lsqa_common_path}"

status=0

printf '1..1\n'
set -v

image_path="${top_srcdir}/tests/data/corrupted/metadata_noise.jpg"
if lsqa_expect_low_quality_or_fail "${image_path}" \
    "metadata_noise.jpg" "${ARTIFACT_LOCAL_DIR}"; then
    pass 1 "metadata noise rejected or scored low"
else
    fail 1 "metadata noise unexpectedly accepted"
fi

exit "${status}"
