#!/bin/sh
# Policy: docs/quality/color-evaluation.md
# Verify auto definition compatibility.
set -eux
echo "1..1"
set -v

expected=$(${SIXEL_RUNTIME-} "${LSQA_PATH}" -m DE00_max "${TOP_SRCDIR}/tests/data/inputs/snake_64.bmp" "${TOP_SRCDIR}/tests/data/inputs/snake_64.six") || {
    printf '\n%s\n' "not ok 1 - default comparison failed"
    exit 0
}
actual=$(${SIXEL_RUNTIME-} "${LSQA_PATH}" -R auto -T auto -m DE00_max "${TOP_SRCDIR}/tests/data/inputs/snake_64.bmp" "${TOP_SRCDIR}/tests/data/inputs/snake_64.six") || {
    printf '\n%s\n' "not ok 1 - auto comparison failed"
    exit 0
}
# Compare finite color-error values to observe CMS routing directly.
case "${actual}:${expected}" in
    *nan*|*inf*|:*)
        printf '\n%s\n' "not ok 1 - CMS comparison must produce finite values"
        exit 0
        ;;
esac
test "$actual" = "$expected" || {
    printf '\n%s\n' "not ok 1 - auto changed compatibility behavior"
    exit 0
}
printf '\n%s\n' "ok 1 - auto definition compatibility"
exit 0
