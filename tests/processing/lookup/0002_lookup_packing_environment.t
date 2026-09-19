#!/bin/sh
# Pin the existing 5bit/6bit dense-LUT packing environment contract.

set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    echo "1..0 # SKIP img2sixel is disabled in this build"
    exit 0
}

echo "1..1"
set -v

input_image="${TOP_SRCDIR}/tests/data/inputs/snake_16.png"

bit5_trace=$(set +xv; SIXEL_TRACE_TOPIC=lookup_contract \
    ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" -L builtin:cms_engine=none! --threads=1 -v --precision=8bit -Wgamma \
    --env "SIXEL_LOOKUP_PACKING=MoRtOn" -p 16 "-~5bit" \
    "${input_image}" 2>&1 >/dev/null) || {
    echo "not ok" 1 - "5bit packing environment conversion failed"
    exit 0
}
case "${bit5_trace}" in *work=rgb888**) true ;; *) false ;; esac || {
    echo "not ok 1 - byte lookup did not use rgb888 working samples"
    exit 0
}
case "${bit5_trace}" in *LSXLUT2\|phase=palette-apply\|selected=lookup/5bit.8bit\|explicit=1**) true ;; *) false ;; esac || {
    echo "not ok 1 - explicit byte lookup missed palette application"
    exit 0
}

case "${bit5_trace}" in *LSXLUT1\|policy=5bit\|packing=morton**) true ;; *) false ;; esac || {
    echo "not ok" 1 - "5bit packing environment was not consumed"
    exit 0
}

bit6_trace=$(set +xv; SIXEL_TRACE_TOPIC=lookup_contract \
    ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" -L builtin:cms_engine=none! --threads=1 -v --precision=8bit -Wgamma \
    --env "SIXEL_LOOKUP_PACKING=MoRtOn" -p 16 "-~6bit" \
    "${input_image}" 2>&1 >/dev/null) || {
    echo "not ok" 1 - "6bit packing environment conversion failed"
    exit 0
}
case "${bit6_trace}" in *work=rgb888**) true ;; *) false ;; esac || {
    echo "not ok 1 - byte lookup did not use rgb888 working samples"
    exit 0
}
case "${bit6_trace}" in *LSXLUT2\|phase=palette-apply\|selected=lookup/6bit.8bit\|explicit=1**) true ;; *) false ;; esac || {
    echo "not ok 1 - explicit byte lookup missed palette application"
    exit 0
}

case "${bit6_trace}" in *LSXLUT1\|policy=6bit\|packing=morton**) true ;; *) false ;; esac || {
    echo "not ok" 1 - "6bit packing environment was not consumed"
    exit 0
}

echo "ok" 1 - "lookup packing environment remains case insensitive"
exit 0
