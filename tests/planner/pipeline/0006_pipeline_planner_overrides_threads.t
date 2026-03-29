#!/bin/sh
# TAP test: override thread split is emitted.

set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n";
    exit 0
}


echo "1..1"
set -v
mkdir -p "${ARTIFACT_LOCAL_DIR}"

ppm_tall="${TOP_SRCDIR}/tests/data/inputs/tall.ppm"

pipeline_log=$(
    ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" --env SIXEL_DITHER_PARALLEL_THREADS_MAX=1 \
                  --env SIXEL_DITHER_PARALLEL_BAND_WIDTH=9 \
                  --env SIXEL_DITHER_PARALLEL_BAND_OVERWRAP=4 \
                  --env SIXEL_THREADS=6 \
                  -v -o "${ARTIFACT_LOCAL_DIR}/tall.six" "${ppm_tall}" 2>&1) || {
    echo "not ok" 1 - "override thread split run failed"
    exit 0
}
printf '%s' "${pipeline_log}" >&2

case "${pipeline_log}" in
    *"band_height="*) ;;
    *)
        echo "not ok" 1 - "override thread split"
        exit 0
        ;;
esac

test -n "${pipeline_log}" || {
    echo "not ok" 1 - "override thread split"
    exit 0
}

echo "ok" 1 - "override thread split"

exit 0
