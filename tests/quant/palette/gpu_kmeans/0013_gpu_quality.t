#!/bin/sh
# Policy: docs/encoder/palette-construction/gpu-kmeans.md

# The reference five-scale MS-SSIM requires dimensions >=176x176.
set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    echo "1..0 # SKIP img2sixel is disabled"
    exit 0
}

probe_status=0
${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" gpu-kmeans/available || probe_status=$?
test "$probe_status" -ne 77 || {
    echo "1..0 # SKIP Metal device is unavailable"
    exit 0
}
echo "1..1"
set -v

test "$probe_status" -eq 0 || {
    echo "not ok 1 - Metal device probe failed"
    exit 0
}

test -d "${ARTIFACT_LOCAL_DIR}" || mkdir -p "${ARTIFACT_LOCAL_DIR}"
trace=$(set +xv; SIXEL_TRACE_TOPIC=kmeans_backend \
    ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" -Gauto -Qkmeans:backend=gpu:seed=1 \
    -Fnone -o "${ARTIFACT_LOCAL_DIR}/gpu.six" \
    "${TOP_SRCDIR}/tests/data/inputs/snake_192.png" 2>&1) || {
    echo "not ok 1 - GPU quality conversion failed"
    exit 0
}
case "$trace" in *LSXKMEANS1\|requested=gpu\|effective=metal\|*) true ;; *) false ;; esac || {
    echo "not ok 1 - GPU quality test did not execute Metal"
    printf '# %s\n' "$trace"
    exit 0
}
${SIXEL_RUNTIME-} "${LSQA_PATH}" -b MS-SSIM:0.98 \
    "${TOP_SRCDIR}/tests/data/inputs/snake_192.png" \
    "${ARTIFACT_LOCAL_DIR}/gpu.six" >/dev/null || {
    echo "not ok 1 - GPU palette quality is below MS-SSIM 0.98"
    exit 0
}
echo "ok 1 - GPU palette meets MS-SSIM 0.98"
exit 0
