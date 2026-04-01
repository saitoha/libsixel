#!/bin/sh
# TAP test: mapfile import rejects palette streams larger than 16 MiB.

set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n"
    exit 0
}

echo "1..1"
set -v
test -d "${ARTIFACT_LOCAL_DIR}" || mkdir -p "${ARTIFACT_LOCAL_DIR}"

snake_png="${TOP_SRCDIR}/tests/data/inputs/snake_64.png"
act_palette="${TOP_SRCDIR}/tests/data/inputs/mapfile/act-oversized-17mib.act"

msg=$(set +xv; ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
          -m "${act_palette}" "${snake_png}" \
          -o/dev/null 2>&1) && {
    echo "not ok" 1 - "oversized mapfile stream unexpectedly succeeded"
    exit 0
}

test "${msg#*sixel_palette_read_stream: palette input exceeds 16 MiB limit.}" != "${msg}" || {
    echo "not ok" 1 - "missing oversized stream diagnostic"
    exit 0
}

echo "ok" 1 - "mapfile stream larger than 16 MiB is rejected"

exit 0
