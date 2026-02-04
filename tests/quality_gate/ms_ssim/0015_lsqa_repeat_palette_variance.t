#!/bin/sh
# Check palette PNG stability across repeated lsqa runs.

set -eu

if [ "${VERBOSE:-0}" -eq 1 ]; then
    set -x
fi

test_name=$(basename "$0")
test_dir=$(CDPATH=; cd "$(dirname "$0")" && pwd)
category_name=$(basename "$(dirname "${test_dir}")")


lsqa_common_path="${TOP_SRCDIR}/tests/lib/sh/lsqa/lsqa_common.sh"
LSQA_HELPER_DIR=$(CDPATH=; cd "$(dirname "${lsqa_common_path}")" && pwd)
export LSQA_HELPER_DIR
. "${lsqa_common_path}"


status=0


artifact_root=${ARTIFACT_ROOT:-"$(pwd)/_artifacts"}
artifact_test_dir=$(dirname "$0")
artifact_dir="${artifact_root}/${artifact_test_dir}/${test_name}"
mkdir -p "${artifact_dir}"

printf '1..1\n'
set -v

image_path="${top_srcdir}/tests/data/inputs/formats/palette.png"
if lsqa_assert_repeat_stability "${image_path}" "palette.png" "${artifact_dir}"; then
    pass 1 "palette repeat variance within tolerance"
else
    fail 1 "palette repeat variance exceeded tolerance"
fi

exit "${status}"
