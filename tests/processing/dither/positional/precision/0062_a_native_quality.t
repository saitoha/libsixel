#!/bin/sh
# Check native positional palette application against the 64px reference.
# Policy: docs/encoder/palette-application/dithering/README.md
# The reference five-scale MS-SSIM requires dimensions >=176x176.
set -eux

test "${HAVE_IMG2SIXEL-0}" = 1 -a "${HAVE_LIBPNG-0}" = 1 || {
    echo "1..0 # SKIP img2sixel or libpng unavailable"
    set -v
    exit 0
}

echo "1..1"
set -v

test -d "${ARTIFACT_LOCAL_DIR}" || mkdir -p "${ARTIFACT_LOCAL_DIR}"
trace=$(set +xv; SIXEL_TRACE_TOPIC=dither_contract \
    ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" --threading-policy=1 \
    -L 'libpng:cms_engine=none!' --precision-policy=float32 \
    -Q kmeans:seed=1 -X linear -W oklab -U gamma \
    -F none -a off -_ none '-~none' -d a_dither -t rgb \
    --sampling-policy=full-frame --binning-policy=none \
    -o "${ARTIFACT_LOCAL_DIR}/output.six" \
    "${TOP_SRCDIR}/tests/data/inputs/snake_192.png" 2>&1) || {
    echo "not ok 1 - native a_dither encoding"
    printf '# %s\n' "${trace}"
    exit 0
}

case "${trace}" in *"diffuse=a_dither|"*"rgb101=1|"*)
    true ;; *) false ;;
esac || {
    echo "not ok 1 - native a_dither dispatch"
    printf '# %s\n' "${trace}"
    exit 0
}

${SIXEL_RUNTIME-} "${LSQA_PATH}" -D -Tsrgb -Wlinear -Pfloat32 \
    -b MS-SSIM:0.98 "${TOP_SRCDIR}/tests/data/inputs/snake_192.png" \
    "${ARTIFACT_LOCAL_DIR}/output.six" >/dev/null || {
    echo "not ok 1 - native a_dither quality"
    exit 0
}

echo "ok 1 - native a_dither quality"
exit 0
