#!/bin/sh
# TAP test verifying -#/--cms-engine rejects unknown values.

set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n";
    exit 0
}


echo "1..1"
set -v

msg=$(set +xv; ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" -# foo \
    "${TOP_SRCDIR}/tests/data/inputs/snake_64.png" \
    -o/dev/null 2>&1) && {
    echo "not ok" 1 - "unknown --cms-engine value unexpectedly succeeded"
    exit 0
}

case "${msg}" in
    *"-#,--cms-engine"* )
        ;;
    *)
        echo "not ok" 1 - "missing --cms-engine option context in diagnostic"
        exit 0
        ;;
esac

case "${msg}" in
    *"cms-engine accepts none, auto, builtin, lcms2, or colorsync."*)
        ;;
    *)
        echo "not ok" 1 - "missing valid-value diagnostics for --cms-engine"
        exit 0
        ;;
esac

echo "ok" 1 - "unknown --cms-engine value is rejected"
exit 0
