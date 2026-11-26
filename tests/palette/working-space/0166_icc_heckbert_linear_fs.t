#!/bin/sh
# Policy: docs/encoder/palette-application/working-colorspace/README.md
# Check ICC heckbert representatives and linear RGB raster FS.
set -eux
echo '1..1'
set -v

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf '\n%s\n' 'ok 1 # SKIP img2sixel unavailable'
    exit 0
}
trace=$(SIXEL_TRACE_TOPIC=loader,palette_contract,dither_contract ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
    --threading-policy=greedy:global_budget=1 --precision-policy=float32 \
    -7 -Lbuiltin! -Usrgb -_none -~none -Fnone -trgb \
    -Qheckbert:cut=median -fnorm -saverage \
    -p2 "-Xicc:${TOP_SRCDIR}/tests/data/color-numeric/display-p3.icc" \
    -Wsrgb-linear -aoff -dfs:scan=raster:deadzone=0 -0none \
    --binning-policy=none --sampling-policy=full-frame \
    "${TOP_SRCDIR}/tests/data/color-numeric/icc-heckbert-fs-samples.ppm" 2>&1) || {
    printf '%s\n' "$trace" >&2
    printf '\n%s\n' 'not ok 1 - heckbert ICC encode'
    exit 0
}
case "$trace" in *'LSXBPS1|quantizer_requested=1|quantizer_effective=1|'*) true ;; *) false ;; esac || {
    printf '\n%s\n' 'not ok 1 - expected heckbert solver'
    exit 0
}
case "$trace" in *'LSXDTH1|rc=0|diffuse=fs|scan=raster|'*'|apply_ok=1|'*) true ;; *) false ;; esac || {
    printf '\n%s\n' 'not ok 1 - expected raster FS application'
    exit 0
}
${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" cli/0036_cli_color_wire \
    icc-heckbert-linear-fs "$trace" || {
    printf '\n%s\n' 'not ok 1 - heckbert ICC representatives and linear FS pixels'
    exit 0
}
printf '\n%s\n' 'ok 1 - heckbert ICC representatives and linear FS pixels'
exit 0
