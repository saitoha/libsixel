#!/bin/sh
# Policy: docs/encoder/output/output-colorspace.md
# Compare two-color SIXEL with a PNG whose ICC already encodes the affine map.
set -eux
echo '1..1'
set -v

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf '\n%s\n' 'ok 1 # SKIP converter or loader unavailable'
    exit 0
}
data="${TOP_SRCDIR}/tests/data/color-numeric"
export SIXEL_LOADER_CMS_BPC=0 SIXEL_OUTPUT_COLORSPACE_ICC_BPC=0
export SIXEL_LOADER_CMS_RENDERING_INTENT=relative SIXEL_LOADER_PREFER_8BIT=0
set -- "-Lbuiltin:Ebuiltin!" -Acomposite -Qkmeans:seed=1 -p2 \
    -Xgamma -Wgamma -_none -Fnone -aoff -dnone -trgb
managed=$(${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" "$@" \
    -U "icc:P${data}/output-bpc.icc:bpc=1" "${data}/output-bpc-rgba16.png") || {
    printf '\n%s\n' 'not ok 1 - builtin BPC conversion failed'
    exit 0
}
reference=$(${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" "$@" \
    -U "icc:P${data}/output-bpc.icc" "${data}/output-bpc-rgba16-reference.png") || {
    printf '\n%s\n' 'not ok 1 - builtin reference conversion failed'
    exit 0
}
test -n "$managed" -a "$managed" = "$reference" || {
    printf '\n%s\n' 'not ok 1 - builtin output BPC differs from mapped source before reduction'
    exit 0
}
printf '\n%s\n' 'ok 1 - builtin output BPC reaches two-color SIXEL'
exit 0
