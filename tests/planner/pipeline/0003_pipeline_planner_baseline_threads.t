#!/bin/sh
# TAP test: baseline thread split is emitted.

set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n";
    exit 0
}


echo "1..1"
set -v
mkdir -p "${ARTIFACT_LOCAL_DIR}"

ppm_small="${TOP_SRCDIR}/tests/data/inputs/small.ppm"

pipeline_log=$(${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" --env SIXEL_THREADS=4 -v -o "${ARTIFACT_LOCAL_DIR}/small.six" "${ppm_small}" 2>&1) || {
    echo "not ok" 1 - "baseline thread split run failed"
    exit 0
}
printf '%s' "${pipeline_log}" >&2

printf '%s\n' "${pipeline_log}" | awk '/band_height=/{ found = 1; exit } END{ if (!found) exit 1 }' || {
    echo "not ok" 1 - "baseline thread split"
    exit 0
}

echo "ok" 1 - "baseline thread split"

exit 0
