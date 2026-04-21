#!/bin/sh
# TAP test verifying quantize choices keep "name -> description" in source.

set -eux

test -f "${TOP_SRCDIR}/converters/img2sixel.c" || {
    printf "1..0 # SKIP missing converters/img2sixel.c\n"
    exit 0
}

echo "1..1"
set -v
set +xv

help_block=''
status=0

help_block=$(cat "${TOP_SRCDIR}/converters/img2sixel.c" 2>/dev/null) || \
    status=$?

test "${status}" -eq 0 || {
    echo "not ok" 1 - "failed to load quantize help block from img2sixel.c"
    exit 0
}

test "${help_block#*:algo=NAME (:a=NAME) choose k-medoids solver:*auto      -> adaptive*sample    -> CLARA:*random    -> CLARANS:*bandit    -> BanditPAM:*}" != "${help_block}" || {
    echo "not ok" 1 - "medoids algo description lines are missing in source help"
    exit 0
}

test "${help_block#*:inittype=TYPE (:i=TYPE) choose k-means seed mode:*auto -> choose seed mode*}" != "${help_block}" || {
    echo "not ok" 1 - "kmeans inittype description lines are missing in source help"
    exit 0
}

echo "ok" 1 - "quantize choices keep name -> description source contract"
exit 0
