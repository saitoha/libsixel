#!/bin/sh
# Verify --baseline=METRIC:VALUE accepts the long option with '='.

set -eux

. "${TOP_SRCDIR}/tests/_lib/sh/common.sh"

printf '1..1\n'
set -v

image_ref="${TOP_SRCDIR}/tests/data/inputs/snake_64.bmp"
image_out="${TOP_SRCDIR}/tests/data/inputs/snake_64.six"
value=$(run_lsqa --metrics=MS-SSIM "${image_ref}" "${image_out}" | tr -d \\r) || {
    fail 1 "failed to calculate baseline source metric"
    exit 0
}

run_lsqa --baseline="MS-SSIM:${value}" "${image_ref}" "${image_out}" >/dev/null || {
    fail 1 "--baseline= should accept METRIC:VALUE"
    exit 0
}

pass 1 "--baseline= accepted METRIC:VALUE"
exit 0
