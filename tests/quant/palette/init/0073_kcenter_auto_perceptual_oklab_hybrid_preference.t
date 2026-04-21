#!/bin/sh
# TAP test: static contract for auto+perceptual+OKLab hybrid preference.

set -eux

test -f "${TOP_SRCDIR}/tests/quant/palette/0003_kcenter_constraints.c" || {
    printf "1..0 # SKIP missing tests/quant/palette/0003_kcenter_constraints.c\n"
    exit 0
}

echo "1..1"
set -v
set +xv

source_file="${TOP_SRCDIR}/tests/quant/palette/0003_kcenter_constraints.c"
case_block=''
dispatch_block=''
status=0

case_block=$(sed -n '1768,2005p' "${source_file}" 2>/dev/null) || status=$?
test "${status}" -eq 0 || {
    echo "not ok" 1 - "failed to load kcenter auto-perceptual case block"
    exit 0
}

test "${case_block#*test_run_auto_perceptual_oklab_hybrid_preference_case*}" \
    != "${case_block}" || {
    echo "not ok" 1 - "missing auto-perceptual-OKLab preference case function"
    exit 0
}

test "${case_block#*SIXEL_PALETTE_KCENTER_SPACE_POLICY_PERCEPTUAL*}" \
    != "${case_block}" || {
    echo "not ok" 1 - "missing perceptual space policy override in auto case"
    exit 0
}

test "${case_block#*SIXEL_PALETTE_KCENTER_ALGO_HYBRID*}" \
    != "${case_block}" || {
    echo "not ok" 1 - "missing hybrid strategy branch in auto-perceptual case"
    exit 0
}

test "${case_block#*test_palette_equal(auto_perceptual_palette*}" \
    != "${case_block}" || {
    echo "not ok" 1 - "missing auto-perceptual vs hybrid equality contract"
    exit 0
}

test "${case_block#*test_palette_equal(auto_legacy_palette*}" \
    != "${case_block}" || {
    echo "not ok" 1 - "missing auto-legacy vs fft equality contract"
    exit 0
}

dispatch_block=$(sed -n '2020,2065p' "${source_file}" 2>/dev/null) || status=$?
test "${status}" -eq 0 || {
    echo "not ok" 1 - "failed to load kcenter dispatcher block"
    exit 0
}

test "${dispatch_block#*auto-perceptual-oklab-hybrid-preference*}" \
    != "${dispatch_block}" || {
    echo "not ok" 1 - "missing auto-perceptual-OKLab dispatcher key"
    exit 0
}

echo "ok" 1 - "kcenter auto-perceptual-OKLab hybrid preference static contract"
exit 0
