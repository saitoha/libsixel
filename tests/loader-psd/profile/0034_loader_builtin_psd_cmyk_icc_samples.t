#!/bin/sh
# Test-plan: docs/testing/builtin-loader-coverage.md
# Policy: docs/loader/builtin/psd.md
# Compare decoded CMYK ICC display samples with an independent PNG.
set -eux
echo "1..1"
set -v

# The independent sRGB PNG rounds once to bytes: allow half a level plus
# float roundoff. Require the actual source-domain builtin ICC application.
trace=$(SIXEL_TRACE_TOPIC=loader \
    SIXEL_LOADER_CMS_RENDERING_INTENT=relative! ${SIXEL_RUNTIME-} \
    "${TEST_RUNNER_PATH}" loader/image_display_values \
    "${TOP_SRCDIR}/tests/data/color-numeric/cmyk-reference.png" \
    "${TOP_SRCDIR}/tests/data/color-numeric/cmyk.psd" \
    'builtin:cms_engine=builtin!' 0.51 2>&1) || {
    echo "not ok 1 - independent PSD CMYK display samples differ: ${trace}"
    exit 0
}
case "${trace}" in
    *"event=apply|engine=builtin|path=extended-rgb|src=5|dst=4|ok=1"*) ;;
    *)
        echo "not ok 1 - source-domain CMYK ICC was not applied by builtin"
        exit 0
        ;;
esac
echo "ok 1 - independent PSD CMYK display samples match with builtin CMS"
exit 0
