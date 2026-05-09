#!/bin/sh
# Verify parse error when -m specifies an ambiguous metric prefix.

set -eux


printf '1..1\n'
set -v

image_ref="${TOP_SRCDIR}/tests/data/inputs/snake_64.bmp"
image_out="${TOP_SRCDIR}/tests/data/inputs/snake_64.six"
status=0
err_msg=''

set +e
err_msg=$(set +xv; ${SIXEL_RUNTIME-} "${LSQA_PATH}" -m C \
    "${image_ref}" "${image_out}" 2>&1 >/dev/null)
status=$?
set -e

test "${status}" -eq 2 || {
    echo "not ok" 1 - "ambiguous -m metric prefix was not rejected as expected"
    exit 0
}

err_tail=${err_msg#*'ambiguous prefix "c"'}
test "${err_tail}" != "${err_msg}" || {
    echo "not ok" 1 - "ambiguous -m metric prefix detail was missing"
    exit 0
}

err_tail=${err_msg#*clip_l_ref}
test "${err_tail}" != "${err_msg}" || {
    echo "not ok" 1 - "ambiguous -m metric prefix candidates were missing"
    exit 0
}

echo "ok" 1 - "ambiguous -m metric prefix was rejected"
exit 0
