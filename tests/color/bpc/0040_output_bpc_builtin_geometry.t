#!/bin/sh
# Preserve the decoded RGB and processing history through frame geometry.
set -eux
echo '1..1'
set -v

SIXEL_LOADER_CMS_BPC=0 SIXEL_LOADER_CMS_RENDERING_INTENT=relative \
SIXEL_LOADER_PREFER_8BIT=0 SIXEL_ALPHA_POLICY=composite \
${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" bpc/output_float 'builtin:Ebuiltin!' \
    "${TOP_SRCDIR}/tests/data/color-numeric/output-bpc-rgba16.png" geometry || {
    printf '\n%s\n' 'not ok 1 - builtin output BPC survives clone, clip and resize'
    exit 0
}
printf '\n%s\n' 'ok 1 - builtin output BPC survives clone, clip and resize'
exit 0
