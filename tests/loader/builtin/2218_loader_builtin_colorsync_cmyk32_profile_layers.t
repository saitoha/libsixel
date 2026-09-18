#!/bin/sh
# Policy: docs/loader/color-management.md
# Test-plan: docs/testing/builtin-loader-coverage.md
# independent correct-ICC and original-input CMYK32 fallback
set -eux
result=0
trace=$(SIXEL_LOADER_CMS_RENDERING_INTENT=relative! SIXEL_TRACE_TOPIC=loader,loader_contract ${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" loader/0202_colorsync_cmyk32_profile_layers "${TOP_SRCDIR}/tests/data/color-numeric/profile-format-layers-valid.psd" "${TOP_SRCDIR}/tests/data/color-numeric/profile-format-layers-none.psd" "${TOP_SRCDIR}/tests/data/color-numeric/profile-format-layers-mismatch.psd" 2>&1) || result=$?
test "$result" -ne 77 || {
    echo "1..0 # SKIP required CMS or loader unavailable"
    set -v
    exit 0
}
echo "1..1"
set -v
test "$result" -eq 0 || {
    echo "$trace" >&2
    echo "not ok 1 - independent correct-ICC and original-input CMYK32 fallback"
    exit 0
}
case "${trace}" in *"LSXCMS2|event=apply|engine=colorsync|path=external|src=7|dst=4|ok=1"*) true ;; *) false ;; esac || {
    echo "$trace" >&2
    echo "not ok 1 - expected actual ColorSync apply outcome"
    exit 0
}
case "${trace}" in *"LSXCMS2|event=apply|engine=colorsync|path=external|src=7|dst=4|ok=0"*) true ;; *) false ;; esac || {
    echo "$trace" >&2
    echo "not ok 1 - expected actual ColorSync apply outcome"
    exit 0
}
echo "ok 1 - 0202_colorsync_cmyk32_profile_layers independent correct-ICC and original-input CMYK32 fallback"
exit 0
