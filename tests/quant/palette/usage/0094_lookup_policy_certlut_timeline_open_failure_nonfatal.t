#!/bin/sh
# Verify logger open failures do not break non-VPTE conversion.
set -eux

. "${TOP_SRCDIR}/tests/_lib/sh/common.sh"

config_macro_defined HAVE_IMG2SIXEL || skip_all "img2sixel is disabled in this build"

echo "1..1"
set -v

small_ppm="${TOP_SRCDIR}/tests/data/inputs/small.ppm"
output_sixel="${ARTIFACT_LOCAL_DIR}/timeline-open-failure.six"
invalid_log_path="${ARTIFACT_LOCAL_DIR}/missing-log-dir/timeline.json"

if run_img2sixel --env SIXEL_LOG_PATH="${invalid_log_path}" \
        --lookup-policy=certlut -p 4 -o "${output_sixel}" "${small_ppm}"; then
    if [ -s "${output_sixel}" ]; then
        pass 1 "conversion succeeds when logger path is not writable"
    else
        fail 1 "conversion succeeded but output file is empty"
    fi
else
    fail 1 "conversion failed when logger open returned runtime error"
fi

exit 0
