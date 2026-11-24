#!/bin/sh
# Test-plan: docs/testing/builtin-loader-coverage.md
# Verify indexed PNG near reqcolors threshold falls back to RGB by checking
# verbose planner output.

set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n";
    exit 0
}


printf '1..1\n'
set -v

reqcolors=253
image_path="${TOP_SRCDIR}/tests/data/inputs/formats/snake-png-pal8.png"

planner_log=$(
    set +xv
    ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" -v \
                  --env SIXEL_ENCODER_THREADING_POLICY=greedy --env SIXEL_ENCODER_THREADING_GLOBAL_BUDGET=1 \
                  -Lbuiltin! -p"${reqcolors}" \
                  "${image_path}" 2>&1 >/dev/null
) || {
    printf '\n%s\n' "not ok 1 - builtin indexed png reqcolors fallback failed"
    exit 0
}

case "${planner_log}" in
    *"formats: source=rgb888 work=rgb888"*)
        ;;
    *)
        printf '\n%s\n' "not ok 1 - builtin indexed png did not take rgb fallback path"
        exit 0
        ;;
esac

printf '\n%s\n' "ok 1 - builtin indexed png reqcolors uses rgb fallback path"
exit 0
