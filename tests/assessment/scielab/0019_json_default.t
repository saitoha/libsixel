#!/bin/sh
# Policy: docs/quality/metrics.md
set -eux
echo "1..1"
set -v

value=$(${SIXEL_RUNTIME-} "${LSQA_PATH}" -L 'builtin!' --cms-engine=none "${TOP_SRCDIR}/tests/data/inputs/snake_16.bmp" "${TOP_SRCDIR}/tests/data/inputs/snake_16.bmp") || {
    printf '\n%s\n' "not ok 1 - scielab default JSON"
    exit 0
}
case "${value}" in
    *'"ppd": 60'*'"S-CIELAB": 0.000000'*'"S-CIELAB_max": 0.000000'*) ;;
    *) printf '\n%s\n' "not ok 1 - scielab JSON contract"; exit 0 ;;
esac

printf '\n%s\n' "ok 1 - scielab default JSON metrics and viewing geometry"
exit 0
