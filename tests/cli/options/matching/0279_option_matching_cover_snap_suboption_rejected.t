#!/bin/sh
# Verify that snap configuration is not accepted by --cover-policy.
# Policy: docs/functionality/snap-policy.md

set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n"
    exit 0
}

echo "1..1"
set -v

result=0
msg=$(
    set +xv
    ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
        --cover-policy=auto:snap_target=reversible \
        "${TOP_SRCDIR}/tests/data/inputs/small.ppm" \
        -o /dev/null 2>&1
) || result=$?

test "${result}" -eq 2 || {
    echo "not ok" 1 - "cover snap suboption exit status mismatch"
    exit 0
}

test "${msg#*unknown suboption key*}" != "${msg}" || {
    echo "not ok" 1 - "missing removed cover snap diagnostic"
    exit 0
}

test "${msg#*snap_target*}" != "${msg}" || {
    echo "not ok" 1 - "removed cover snap diagnostic lacks key name"
    exit 0
}

echo "ok" 1 - "cover policy rejects removed snap configuration"
exit 0
