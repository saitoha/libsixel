#!/bin/sh
# Test-plan: docs/testing/builtin-loader-coverage.md
# Verify replay replacement/locked-skip traces do not appear after commit.
# Fixture/expected regeneration command:
#   python3 tests/data/psd-tools/generate_psdtools_hybrid_assets.py --download

set -eux

: "${IMG2SIXEL_PATH:=${TOP_BUILDDIR}/converters/img2sixel}"

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n"
    exit 0
}

echo "1..1"
set -v
set +x

# Check for a post-commit character without extracting a long trace suffix.

input_psd="${TOP_SRCDIR}/tests/data/psd-tools/psdtools_effects_stroke_composite.psd"
trace_output=''
diag_line=''
commit_marker='builtin PSD: compositing deferred offscreen clipped group buffer to canvas'
command_status=0
nl='
'

trace_output=$(set +xv; ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
    --env SIXEL_TRACE_TOPIC=psd_decode \
    --env SIXEL_PSD_TRACE_ONLY=1 \
    -Lbuiltin:Eauto! -o /dev/null "${input_psd}" 2>&1) || \
    command_status=$?

test "${command_status}" -eq 0 || {
    echo "not ok" 1 - "effects/stroke-composite decode failed"
    exit 0
}


{ test -n "${commit_marker}" && case "${trace_output}" in *"${commit_marker}"*) true ;; *) false ;; esac; } || {
    echo "not ok" 1 - "effects/stroke-composite missing deferred group commit trace"
    exit 0
}

case "${trace_output}" in *"${commit_marker}"?*) true ;; *) false ;; esac || {
    echo "not ok" 1 - \
        "effects/stroke-composite missing post-commit trace tail"
    exit 0
}

diag_line=${trace_output#*LSXPSD1\|}
case "${trace_output}" in *LSXPSD1\|*) true ;; *) false ;; esac || {
    echo "not ok" 1 - "effects/stroke-composite missing LSXPSD1 contract header"
    exit 0
}

diag_line="LSXPSD1|${diag_line}"
diag_line=${diag_line%%"${nl}"*}

case "${diag_line}" in *FX_DEFERRED_OVERLAY_REPLAY_REPLACE**) false ;; *) true ;; esac || {
    echo "not ok" 1 - \
        "effects/stroke-composite emitted replay replacement code in normal flow"
    exit 0
}

case "${diag_line}" in *FX_DEFERRED_OVERLAY_REPLAY_SKIP_LOCKED**) false ;; *) true ;; esac || {
    echo "not ok" 1 - \
        "effects/stroke-composite emitted replay lock skip code in normal flow"
    exit 0
}

case "${diag_line}" in *FX_DEFERRED_POST_COMMIT_REPLAY_BLOCKED**) false ;; *) true ;; esac || {
    echo "not ok" 1 - \
        "effects/stroke-composite emitted post-commit replay-block code in normal flow"
    exit 0
}

echo "ok" 1 - \
    "effects/stroke-composite keeps replay replacement/lock codes out of normal flow"
exit 0
