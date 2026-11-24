#!/bin/sh
# Policy: docs/encoder/output/output-colorspace.md
# Verify uncompensated ICC samples and file-background composition in float.
set -eux
echo '1..1'
set -v

SIXEL_LOADER_CMS_BPC=0 SIXEL_LOADER_CMS_RENDERING_INTENT=relative \
SIXEL_LOADER_PREFER_8BIT=0 SIXEL_ALPHA_POLICY=composite \
${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" bpc/output_float 'builtin:Ebuiltin!' \
    "${TOP_SRCDIR}/tests/data/color-numeric/output-bpc-rgba16.png" off || {
    printf '\n%s\n' 'not ok 1 - builtin output BPC off float samples'
    exit 0
}
printf '\n%s\n' 'ok 1 - builtin output BPC off float samples'
exit 0
