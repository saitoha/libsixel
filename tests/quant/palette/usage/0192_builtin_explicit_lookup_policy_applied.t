#!/bin/sh
# Policy: docs/encoder/palette-construction/fixed-palettes.md
# Verify explicit 5bit lookup reaches built-in palette application.
set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n"
    exit 0
}

echo "1..1"
set -v

input_image="${TOP_SRCDIR}/tests/data/inputs/snake_16.png"

trace=$(set +xv; SIXEL_TRACE_TOPIC=lookup_contract \
    ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" -L builtin:cms_engine=none! --threading-policy=balanced:global_budget=1 -v --precision-policy=8bit -Wgamma -Goff -dnone \
    -bxterm256 --lookup-policy=5bit -o/dev/null "${input_image}" \
    2>&1 >/dev/null) || {
    echo "not ok" 1 - "built-in palette conversion with 5bit lookup failed"
    exit 0
}
case "${trace}" in *work=rgb888**) true ;; *) false ;; esac || {
    echo "not ok 1 - byte lookup did not use rgb888 working samples"
    exit 0
}

case "${trace}" in *LSXLUT2\|phase=palette-apply\|selected=lookup/5bit.8bit\|explicit=1**) true ;; *) false ;; esac || {
    echo "not ok" 1 - "explicit 5bit missed built-in palette application"
    exit 0
}

float_trace=$(set +xv; SIXEL_TRACE_TOPIC=lookup_contract \
    ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" -L builtin:cms_engine=none! --threading-policy=balanced:global_budget=1 -v --precision-policy=float32 -Wgamma -Goff -dnone \
    -bxterm256 --lookup-policy=5bit -o/dev/null "${input_image}" \
    2>&1 >/dev/null) || {
    echo "not ok" 1 - "built-in palette conversion with 5bit lookup failed"
    exit 0
}
case "${float_trace}" in *work=rgb-f32**) true ;; *) false ;; esac || {
    echo "not ok 1 - float lookup did not use rgb-f32 working samples"
    exit 0
}

case "${float_trace}" in *LSXLUT2\|phase=palette-apply\|selected=lookup/5bit.float32\|explicit=1**) true ;; *) false ;; esac || {
    echo "not ok" 1 - "explicit 5bit missed built-in palette application"
    exit 0
}

echo "ok" 1 - "explicit 5bit applies to a built-in palette"
exit 0
