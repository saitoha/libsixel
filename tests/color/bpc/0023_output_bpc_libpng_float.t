#!/bin/sh
# Policy: docs/encoder/output/output-colorspace.md
# Verify relative output BPC and file-background composition in float.
set -eux
echo '1..1'
set -v
test "${HAVE_LIBPNG-}" = 1 || {
    printf '\n%s\n' "ok 1 # SKIP libpng unavailable"
    exit 0
}

SIXEL_LOADER_CMS_BPC=0 SIXEL_LOADER_CMS_RENDERING_INTENT=relative \
SIXEL_LOADER_PREFER_8BIT=0 SIXEL_ALPHA_POLICY=composite \
${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" bpc/output_float 'libpng:Ebuiltin!' \
    "${TOP_SRCDIR}/tests/data/color-numeric/output-bpc-rgba16.png" || {
    printf '\n%s\n' 'not ok 1 - libpng output BPC float samples'
    exit 0
}
printf '\n%s\n' 'ok 1 - libpng output BPC float samples'
exit 0
