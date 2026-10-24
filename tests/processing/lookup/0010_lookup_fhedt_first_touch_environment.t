#!/bin/sh
# Pin the canonical and legacy FHEDT first-touch environment contract.

set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    echo "1..0 # SKIP img2sixel is disabled in this build"
    exit 0
}

# Keep byte-backend coverage independent of default soft headroom.
echo "1..1"
set -v

trace=$(set +xv; SIXEL_TRACE_TOPIC=lookup_contract \
    ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" -L builtin:cms_engine=none! --threading-policy=balanced:global_budget=1 -v --precision-policy=8bit -Wgamma -0hard \
    --env "SIXEL_FHEDT_FIRST_TOUCH=0" \
    --env "SIXEL_LOOKUP_FHEDT_FIRST_TOUCH=1" -p 16 "-~fhedt" \
    "${TOP_SRCDIR}/tests/data/inputs/snake_16.png" 2>&1 >/dev/null) || {
    echo "not ok" 1 - "FHEDT first_touch environment conversion failed"
    exit 0
}
case "${trace}" in *work=rgb888**) true ;; *) false ;; esac || {
    echo "not ok 1 - byte lookup did not use rgb888 working samples"
    exit 0
}
case "${trace}" in *LSXLUT2\|phase=palette-apply\|selected=lookup/fhedt.8bit\|explicit=1**) true ;; *) false ;; esac || {
    echo "not ok 1 - explicit byte lookup missed palette application"
    exit 0
}

case "${trace}" in *LSXFHD1\|precision=8bit\|*first_touch=1**) true ;; *) false ;; esac || {
    echo "not ok" 1 - "canonical FHEDT first_touch did not win"
    exit 0
}

echo "ok" 1 - "canonical FHEDT first_touch precedes its legacy alias"
exit 0
