#!/bin/sh
# Staticcheck: staticcheck-kcenter-auto-perceptual-preference
# Policy: docs/testing/staticcheck.md
# TAP test: static contract for auto+perceptual+OKLab hybrid preference.

set -eux

test -f "${TOP_SRCDIR}/tests/_support/quant/palette/0003_kcenter_constraints.c" || {
    printf "1..0 # SKIP missing tests/_support/quant/palette/0003_kcenter_constraints.c\n"
    exit 0
}

echo "1..1"
set -v
set +xv

source_file="${TOP_SRCDIR}/tests/_support/quant/palette/0003_kcenter_constraints.c"
source_text=''
status=0

source_text=$(cat "${source_file}" 2>/dev/null) || status=$?
test "${status}" -eq 0 || {
    echo "not ok" 1 - "failed to load kcenter constraints source"
    exit 0
}

case "${source_text}" in *test_run_auto_perceptual_oklab_hybrid_preference_case**) true ;; *) false ;; esac || {
    echo "not ok" 1 - "missing auto-perceptual-OKLab preference case function"
    exit 0
}

case "${source_text}" in *SIXEL_PALETTE_KCENTER_SPACE_POLICY_PERCEPTUAL**) true ;; *) false ;; esac || {
    echo "not ok" 1 - "missing perceptual space policy override in auto case"
    exit 0
}

case "${source_text}" in *SIXEL_PALETTE_KCENTER_ALGO_HYBRID**) true ;; *) false ;; esac || {
    echo "not ok" 1 - "missing hybrid strategy branch in auto-perceptual case"
    exit 0
}

case "${source_text}" in *test_palette_equal\(auto_perceptual_palette**) true ;; *) false ;; esac || {
    echo "not ok" 1 - "missing auto-perceptual vs hybrid equality contract"
    exit 0
}

case "${source_text}" in *test_palette_equal\(auto_legacy_palette**) true ;; *) false ;; esac || {
    echo "not ok" 1 - "missing auto-legacy vs fft equality contract"
    exit 0
}

case "${source_text}" in *auto-perceptual-oklab-hybrid-preference**) true ;; *) false ;; esac || {
    echo "not ok" 1 - "missing auto-perceptual-OKLab dispatcher key"
    exit 0
}

echo "ok" 1 - "kcenter auto-perceptual-OKLab hybrid preference static contract"
exit 0
