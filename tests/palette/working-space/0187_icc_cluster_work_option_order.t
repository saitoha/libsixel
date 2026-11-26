#!/bin/sh
# Policy: docs/encoder/palette-application/working-colorspace/README.md
# A separate working ICC preserves the requested clustering profile.
set -eux
echo '1..1'
set -v

test "${HAVE_IMG2SIXEL-0}" = 1 || {
    printf '\n%s\n' 'ok 1 # SKIP img2sixel unavailable'
    exit 0
}
cluster="-Xicc:${TOP_SRCDIR}/tests/data/color-numeric/display-p3.icc"
working="-Wicc:${TOP_SRCDIR}/tests/data/color-numeric/narrow-p3-cluster.icc"
# The Display P3 centroids emit (68,76,75) twice and (58,24,71) once.
# Using the narrow ICC to cluster would emit (55,72,71) for the first pair.
input='P3
3 1
255
241 168 181  88 220 201  147 61 180'
cluster_first=$(set +xv; printf '%s\n' "$input" |
    SIXEL_TRACE_TOPIC=loader ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
        --threading-policy=greedy:B1 --precision-policy=float32 \
        -7 -Lbuiltin! -Ugamma -_none -~none -Fnone -trgb \
        -Qkmeans:seed=1 -p2 "$cluster" "$working" -aoff -dnone \
        --binning-policy=none --sampling-policy=full-frame 2>&1) || {
    printf '%s\n' "$cluster_first" >&2
    printf '\n%s\n' 'not ok 1 - encode with clustering ICC first'
    exit 0
}
working_first=$(set +xv; printf '%s\n' "$input" |
    SIXEL_TRACE_TOPIC=loader ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
        --threading-policy=greedy:B1 --precision-policy=float32 \
        -7 -Lbuiltin! -Ugamma -_none -~none -Fnone -trgb \
        -Qkmeans:seed=1 -p2 "$working" "$cluster" -aoff -dnone \
        --binning-policy=none --sampling-policy=full-frame 2>&1) || {
    printf '%s\n' "$working_first" >&2
    printf '\n%s\n' 'not ok 1 - encode with working ICC first'
    exit 0
}
${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" cli/0036_cli_color_wire \
    pnm-cluster-icc-linear "$cluster_first" || {
    printf '\n%s\n' 'not ok 1 - clustering ICC first must use Display P3 centroids'
    exit 0
}
${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" cli/0036_cli_color_wire \
    pnm-cluster-icc-linear "$working_first" || {
    printf '\n%s\n' 'not ok 1 - working ICC first must use Display P3 centroids'
    exit 0
}
printf '\n%s\n' 'ok 1 - distinct clustering and working ICCs are independent of option order'
exit 0
