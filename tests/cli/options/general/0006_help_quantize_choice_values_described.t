#!/bin/sh
# TAP test verifying quantize choice values use "name -> description".

set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n"
    exit 0
}

echo "1..1"
set -v

set +x
msg=$(${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" -H 2>/dev/null)
status=$?
set -x
test "${status}" -eq 0 || {
    echo "not ok" 1 - "img2sixel -H failed"
    exit 0
}
set +x

test "${msg#*auto\|pam\|sample\|random\|bandit*}" = "${msg}" || {
    echo "not ok" 1 - "medoids algo still uses pipe list format"
    exit 0
}

test "${msg#*auto\|none\|pca*}" = "${msg}" || {
    echo "not ok" 1 - "kmeans inittype still uses pipe list format"
    exit 0
}

test "${msg#*:algo=NAME \(:a=NAME\)*auto      -> adaptive*}" != "${msg}" || {
    echo "not ok" 1 - "medoids algo description lines are missing"
    exit 0
}

test "${msg#*:inittype=TYPE \(:i=TYPE\)*auto -> choose seed mode*}" != "${msg}" || {
    echo "not ok" 1 - "kmeans inittype description lines are missing"
    exit 0
}

echo "ok" 1 - "-H quantize choices use name -> description format"
exit 0
