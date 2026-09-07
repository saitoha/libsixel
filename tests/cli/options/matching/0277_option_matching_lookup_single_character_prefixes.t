#!/bin/sh
# TAP test verifying lookup policies retain one-character unique prefixes.
# Policy: docs/cli/design-policy.md

set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n"
    exit 0
}

echo "1..1"
set -v

for lookup_prefix in a 5 6 n c e f v r m; do
    ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
        --lookup-policy="${lookup_prefix}" -dnone -p16 \
        "${TOP_SRCDIR}/tests/data/inputs/small.ppm" -o/dev/null || {
        echo "not ok" 1 - \
            "lookup prefix ${lookup_prefix} was not accepted"
        exit 0
    }
done

echo "ok" 1 - "lookup policies retain single-character prefixes"
exit 0
