#!/bin/sh
# TAP test verifying -H indents suboption blocks under each quantize model.

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

test "${msg#*kmeans   -> k-means clustering.*sub-option:*:inittype=TYPE*}" != "${msg}" || {
    echo "not ok" 1 - "missing indented kmeans suboption block in -H"
    exit 0
}

test "${msg#*medoids -> k-medoids clustering.*sub-option:*:algo=NAME*}" != "${msg}" || {
    echo "not ok" 1 - "missing indented medoids suboption block in -H"
    exit 0
}

test "${msg#*kmeans   -> k-means clustering.*:prune=POLICY*}" != "${msg}" || {
    echo "not ok" 1 - "missing kmeans prune suboption in -H"
    exit 0
}

test "${msg#*kmeans   -> k-means clustering.*medoids -> k-medoids clustering.*}" != "${msg}" || {
    echo "not ok" 1 - "kmeans/medoids blocks are out of order"
    exit 0
}

echo "ok" 1 - "-H quantize suboption blocks are indented and ordered"
exit 0
