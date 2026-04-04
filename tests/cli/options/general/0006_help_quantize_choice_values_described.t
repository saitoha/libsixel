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

quant="${msg#*-Q MODEL, --quantize-model=MODEL*}"
test "${quant}" != "${msg}" || {
    echo "not ok" 1 - "missing quantize section in -H output"
    exit 0
}

quant="${quant%%-F MODE, --final-merge=MODE*}"

test "${quant#*auto\|pam\|sample\|random\|bandit*}" = "${quant}" || {
    echo "not ok" 1 - "medoids algo still uses pipe list format"
    exit 0
}

test "${quant#*auto\|none\|pca*}" = "${quant}" || {
    echo "not ok" 1 - "kmeans inittype still uses pipe list format"
    exit 0
}

test "${quant#*:algo=NAME \(:a=NAME\)*auto      -> adaptive*}" != "${quant}" || {
    echo "not ok" 1 - "medoids algo description lines are missing"
    exit 0
}

test "${quant#*sample    -> CLARA:*}" != "${quant}" || {
    echo "not ok" 1 - "sample (CLARA) description lines are missing"
    exit 0
}

test "${quant#*random    -> CLARANS:*}" != "${quant}" || {
    echo "not ok" 1 - "random (CLARANS) description lines are missing"
    exit 0
}

test "${quant#*bandit    -> BanditPAM:*}" != "${quant}" || {
    echo "not ok" 1 - "bandit (BanditPAM) description lines are missing"
    exit 0
}

test "${quant#*:inittype=TYPE \(:i=TYPE\)*auto -> choose seed mode*}" != "${quant}" || {
    echo "not ok" 1 - "kmeans inittype description lines are missing"
    exit 0
}

echo "ok" 1 - "-H quantize choices use name -> description format"
exit 0
