#!/bin/sh
# Policy: docs/functionality/animation.md
# Policy: docs/functionality/terminal-macros.md
# Verify static selection preloads only the first animation frame.

set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n"
    exit 0
}

echo "1..1"
set -v

output=$(${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
    -Lbuiltin! -7 -bgray2 -dnone -Efast -g -n7 -S -lauto \
    "${TOP_SRCDIR}/tests/data/inputs/formats/gif-anim-netscape-loop2.gif") || {
    echo "not ok 1 - static selection preloads one animation frame: conversion failed"
    exit 0
}

# The hex bodies include the complete SIXEL envelope, palette, and pixels.
escape=$(printf '\033')
expected="${escape}P7;0;1!z1b507122313b313b323b3123303b323b303b303b3023313b323b33333b33333b333323323b323b36373b36373b363723333b323b3130303b3130303b3130302330402331401b5c${escape}\\"
test "$output" = "$expected" || {
    echo "not ok 1 - static selection preloads one animation frame: unexpected control stream"
    exit 0
}

echo "ok 1 - static selection preloads one animation frame"
exit 0
