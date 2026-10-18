#!/bin/sh
# Observe effective Heckbert build settings for profile compat none.
# Policy: docs/encoder/palette-construction/quantization/README.md
set -eux
echo "1..1"
set -v
trace=$(SIXEL_TRACE_TOPIC=palette_contract ${SIXEL_RUNTIME-} \
    "${IMG2SIXEL_PATH}" -=1 -Goff -p32 -Qheckbert:profile=compat -dnone \
    "${TOP_SRCDIR}/tests/data/inputs/snake_16.png" 2>&1 >/dev/null) || {
    echo "not ok 1 - profile_compat_none conversion"
    exit 0
}
case "$trace" in *'LSXHBC1|rep=3|cut=sse|order=sse|weights=0/0/0'*) true ;; *) false ;; esac || {
    echo "not ok 1 - profile_compat_none effective settings"
    exit 0
}
echo "ok 1 - profile_compat_none"
exit 0
