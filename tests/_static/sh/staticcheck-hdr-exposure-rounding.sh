#!/bin/sh
# Keep the HDR exposure overflow check independent of excess register range.
set -eux
echo '1..1'
set -v
awk -f "$1/tests/_static/awk/hdr-exposure-rounding.awk" \
    "$1/src/loader-common.c" || {
    echo 'not ok 1 - HDR exposure is rounded to double before range validation'
    exit 0
}
echo 'ok 1 - HDR exposure is rounded to double before range validation'
exit 0
