#!/bin/sh
# Verify that disabling palette tables selects the shift-based fallback.

set -eux

echo "1..1"
set -v

trace=$(set +xv; ${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    --env _SIXEL_TEST_PALETTE_DISABLE_TABLES=1 \
    --env SIXEL_TRACE_TOPIC=pixelformat_contract \
    "palette/fallback/0006_palette_fallback_conversion" 2>&1) || \
    trace_status=$?
test "${trace_status:-0}" -eq 0 || {
    printf 'not ok 1 - fallback expansion failed\n'
    exit 0
}

test "${trace#*LSXPIX1|palette_expand=fallback|bpp=1*}" != "${trace}" || {
    printf 'not ok 1 - fallback expansion path was not selected\n'
    exit 0
}

printf 'ok 1 - PAL1 input selects the fallback expansion path\n'
exit 0
