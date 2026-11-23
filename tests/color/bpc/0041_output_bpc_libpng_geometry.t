#!/bin/sh
# Preserve the decoded RGB and processing history through frame geometry.
set -eux
echo '1..1'
set -v
test "${HAVE_LIBPNG-}" = 1 || {
    echo "ok 1 # SKIP libpng unavailable"
    exit 0
}

SIXEL_LOADER_CMS_BPC=0 SIXEL_LOADER_CMS_RENDERING_INTENT=relative \
SIXEL_LOADER_PREFER_8BIT=0 SIXEL_ALPHA_POLICY=composite \
${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" bpc/output_float 'libpng:Ebuiltin!' \
    "${TOP_SRCDIR}/tests/data/color-numeric/output-bpc-rgba16.png" geometry || {
    echo 'not ok 1 - libpng output BPC survives clone, clip and resize'
    exit 0
}
echo 'ok 1 - libpng output BPC survives clone, clip and resize'
exit 0
