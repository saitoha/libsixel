#!/bin/sh
# TAP test: override bands/queue/mode is emitted.

set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n";
    exit 0
}

. "${TOP_SRCDIR}/tests/_lib/sh/common.sh"

echo "1..1"
set -v

ppm_tall="${TOP_SRCDIR}/tests/data/inputs/tall.ppm"

pipeline_log=$(
    run_img2sixel --env SIXEL_DITHER_PARALLEL_THREADS_MAX=1 \
                  --env SIXEL_DITHER_PARALLEL_BAND_WIDTH=9 \
                  --env SIXEL_DITHER_PARALLEL_BAND_OVERWRAP=4 \
                  --env SIXEL_THREADS=6 \
                  -v -o "${ARTIFACT_LOCAL_DIR}/tall.six" "${ppm_tall}" 2>&1) || {
    echo "not ok" 1 - "override bands/queue/mode run failed"
    exit 0
}
printf '%s' "${pipeline_log}" >&2

printf '%s\n' "${pipeline_log}" | awk '/bands=/{ found = 1; exit } END{ if (!found) exit 1 }' || {
    echo "not ok" 1 - "override bands/queue/mode"
    exit 0
}

echo "ok" 1 - "override bands/queue/mode"
exit 0
