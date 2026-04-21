#!/bin/sh
# TAP test verifying quantize help suboption blocks stay defined in source.

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

test "${help_block#*kmeans   -> k-means clustering. sub-option:*:inittype=TYPE*}" != "${help_block}" || {
    echo "not ok" 1 - "missing kmeans suboption block in quantize help source"
    exit 0
}

test "${help_block#*medoids -> k-medoids clustering. sub-option:*:algo=NAME*}" != "${help_block}" || {
    echo "not ok" 1 - "missing medoids suboption block in quantize help source"
    exit 0
}

test "${help_block#*center  -> discrete k-center clustering. sub-option:*:algo=NAME*}" != "${help_block}" || {
    echo "not ok" 1 - "missing center suboption block in quantize help source"
    exit 0
}

echo "ok" 1 - "quantize suboption blocks stay defined in source help contract"
exit 0
