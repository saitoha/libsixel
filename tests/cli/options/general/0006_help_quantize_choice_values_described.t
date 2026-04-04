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
status=0
msg=$(set +xv; ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" -Q: 2>&1) || status=$?
set -x
test "${status}" -ne 0 || {
    echo "not ok" 1 - "img2sixel -Q: unexpectedly succeeded"
    exit 0
}
set +x

test "${msg#*-Q MODEL, --quantize-model=MODEL*}" != "${msg}" || {
    echo "not ok" 1 - "missing quantize section in -Q diagnostic output"
    exit 0
}

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

test "${msg#*sample    -> CLARA:*}" != "${msg}" || {
    echo "not ok" 1 - "sample (CLARA) description lines are missing"
    exit 0
}

test "${msg#*random    -> CLARANS:*}" != "${msg}" || {
    echo "not ok" 1 - "random (CLARANS) description lines are missing"
    exit 0
}

test "${msg#*bandit    -> BanditPAM:*}" != "${msg}" || {
    echo "not ok" 1 - "bandit (BanditPAM) description lines are missing"
    exit 0
}

test "${msg#*:inittype=TYPE \(:i=TYPE\)*auto -> choose seed mode*}" != "${msg}" || {
    echo "not ok" 1 - "kmeans inittype description lines are missing"
    exit 0
}

echo "ok" 1 - "-H quantize choices use name -> description format"
exit 0
