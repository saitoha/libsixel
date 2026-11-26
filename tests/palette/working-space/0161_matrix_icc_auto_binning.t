#!/bin/sh
# Policy: docs/encoder/palette-application/working-colorspace/README.md
# Check matrix/TRC ICC centroids at the SIXEL wire boundary.
set -eux
echo '1..1'
set -v

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf '\n%s\n' 'ok 1 # SKIP img2sixel unavailable'
    exit 0
}
trace=$(SIXEL_TRACE_TOPIC=loader,palette_contract ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
    --threading-policy=greedy:global_budget=1 --precision-policy=float32 \
    -7 -Lbuiltin! -Ugamma -_none -~none -Fnone -trgb \
    -Qkmeans:seed=1:binbits=4 -p2 "-Xicc:${TOP_SRCDIR}/tests/data/color-numeric/display-p3.icc" -Wlinear -aoff -dnone \
    --binning-policy=auto --sampling-policy=full-frame 2>&1 <<'PNM'
P3
3 1
255
243 243 243  245 245 245  247 247 247
PNM
) || {
    printf '%s\n' "$trace" >&2
    printf '\n%s\n' 'not ok 1 - ICC clustering with automatic binning conversion'
    exit 0
}
case "$trace" in
    *'LSXRANGE1|requested=2|safe=3|hard=1|complete=1|eligible=1'*'|candidates=3|'*) ;;
    *) printf '\n%s\n' 'not ok 1 - ICC safe-tone range recovery did not execute'; exit 0 ;;
esac
${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" cli/0036_cli_color_wire \
    pnm-cluster-icc-binning "$trace" || {
    printf '\n%s\n' 'not ok 1 - ICC clustering with automatic binning'
    exit 0
}
printf '\n%s\n' 'ok 1 - ICC clustering with automatic binning'
exit 0
