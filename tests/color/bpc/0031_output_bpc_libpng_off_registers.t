#!/bin/sh
# Policy: docs/encoder/output/output-colorspace.md
# Check RGB101 register definitions for explicit ICC output with BPC off.
set -eux
echo '1..1'
set -v

test "${HAVE_IMG2SIXEL-}" = 1 -a "${HAVE_LIBPNG-}" = 1 || {
    echo 'ok 1 # SKIP converter or loader unavailable'
    exit 0
}
data="${TOP_SRCDIR}/tests/data/color-numeric"
export SIXEL_LOADER_CMS_BPC=0 SIXEL_OUTPUT_COLORSPACE_ICC_BPC=1
export SIXEL_LOADER_CMS_RENDERING_INTENT=relative SIXEL_LOADER_PREFER_8BIT=0
trace=$(SIXEL_TRACE_TOPIC=loader ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
    -7 -Llibpng:Ebuiltin! -Acomposite -Qkmeans:seed=1 -p2 \
    -Xgamma -Wgamma -_none -Fnone -aoff -dnone -trgb \
    -U "icc:P${data}/output-bpc.icc:bpc=0" \
    "${data}/output-bpc-rgba16.png" 2>&1) || {
    printf '%s\n' "$trace" >&2
    echo 'not ok 1 - libpng output BPC off registers conversion'
    exit 0
}
${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" cli/0036_cli_color_wire \
    libpng "$trace" bpc-off-registers || {
    printf '%s\n' "$trace" >&2
    echo 'not ok 1 - libpng output BPC off registers'
    exit 0
}
echo 'ok 1 - libpng output BPC off registers'
exit 0
