#!/bin/sh
# Policy: docs/loader/builtin-cms.md
# Verify hlg color ootf against independent reference values.
set -eux
echo '1..1'
set -v
${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" hdr/0003_hlg_color_ootf || {
    echo 'not ok 1 - hlg_color_ootf'
    exit 0
}
echo 'ok 1 - hlg_color_ootf'
exit 0
