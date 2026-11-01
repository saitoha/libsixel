#!/bin/sh
# Accept independently generated PSD RGB sample values.
set -eux

echo "1..1"
set -v

status=0
${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" loader/image_values \
    "${TOP_SRCDIR}/tests/data/loader/builtin_expected/psd_snake16_multilayer_fill_soco_expected.ppm" \
    "${TOP_SRCDIR}/tests/data/inputs/formats/snake16_cmyk8_missing_composite_multilayer_nonpixel_nopixel_tysh_enginedata_fillcolor_dual_scope_precedence.psd" \
    'builtin!' 1.01 || status=$?
test "${status}" -eq 0 || {
    echo "not ok 1 - Accept independently generated PSD RGB sample values"
    exit 0
}
echo "ok 1 - Accept independently generated PSD RGB sample values"
exit 0
