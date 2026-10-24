#!/bin/sh
# Observe effective Heckbert build settings for profile speed explicit.
# Policy: docs/encoder/palette-construction/quantization/README.md
set -eux
echo "1..1"
set -v
trace=$(SIXEL_TRACE_TOPIC=palette_contract ${SIXEL_RUNTIME-} \
    "${IMG2SIXEL_PATH}" -=balanced:B1 -Goff -p32 -Qheckbert:profile=speed:cut=median:order=population:weights=1/2/3 -scenter \
    "${TOP_SRCDIR}/tests/data/inputs/snake_16.png" 2>&1 >/dev/null) || {
    echo "not ok 1 - profile_speed_explicit conversion"
    exit 0
}
case "$trace" in *'LSXHBC1|rep=1|cut=median|order=population|weights=1/2/3'*) true ;; *) false ;; esac || {
    echo "not ok 1 - profile_speed_explicit effective settings"
    exit 0
}
echo "ok 1 - profile_speed_explicit"
exit 0
