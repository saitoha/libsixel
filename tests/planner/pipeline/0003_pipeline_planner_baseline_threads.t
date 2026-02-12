#!/bin/sh
# TAP test: baseline thread split is emitted.

set -eux

. "${TOP_SRCDIR}/tests/_lib/sh/common.sh"

ensure_converter_available "IMG2SIXEL" "${IMG2SIXEL_PATH}" "img2sixel"

echo "1..1"
set -v

ppm_small="${TOP_SRCDIR}/tests/data/inputs/small.ppm"

pipeline_log=$(run_img2sixel --env SIXEL_THREADS=4 -v -o "${ARTIFACT_LOCAL_DIR}/small.six" "${ppm_small}" 2>&1) || {
    fail 1 "baseline thread split run failed"
    exit 0
}
printf '%s' "${pipeline_log}" >&2

threads_line=$(printf '%s' "${pipeline_log}" | grep "band_height=" | head -n 1) || threads_line=""
printf '%s' "${threads_line}" | grep -q '^[[:space:]]*band_height=' || {
    fail 1 "baseline thread split"
    exit 0
}

pass 1 "baseline thread split"

exit 0
