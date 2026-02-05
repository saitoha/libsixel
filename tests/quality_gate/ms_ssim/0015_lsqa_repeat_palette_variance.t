#!/bin/sh
# Check palette PNG stability across repeated lsqa runs.

set -eu

if [ "${VERBOSE:-0}" -eq 1 ]; then
    set -x
fi

lsqa_common_path="${TOP_SRCDIR}/tests/lib/sh/lsqa/lsqa_common.sh"
. "${lsqa_common_path}"


status=0

printf '1..1\n'
set -v

image_path="${top_srcdir}/tests/data/inputs/formats/palette.png"
if lsqa_assert_repeat_stability "${image_path}" "palette.png" "${ARTIFACT_LOCAL_DIR}"; then
    pass 1 "palette repeat variance within tolerance"
else
    fail 1 "palette repeat variance exceeded tolerance"
fi

exit "${status}"
