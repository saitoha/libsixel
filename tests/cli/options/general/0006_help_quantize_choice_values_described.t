#!/bin/sh
# TAP test verifying quantize choice values use "name -> description".

set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n"
    exit 0
}

echo "1..1"
set -v

set +x
msg=$(${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" -H 2>/dev/null)
status=$?
set -x
test "${status}" -eq 0 || {
    echo "not ok" 1 - "img2sixel -H failed"
    exit 0
}
set +x

printf '%s\n' "${msg}" | awk '
    index($0, "auto|pam|clara|clarans|banditpam") { exit 1 }
    END { exit 0 }
' || {
    echo "not ok" 1 - "medoids algo still uses pipe list format"
    exit 0
}

printf '%s\n' "${msg}" | awk '
    index($0, "auto|none|pca") { exit 1 }
    END { exit 0 }
' || {
    echo "not ok" 1 - "kmeans inittype still uses pipe list format"
    exit 0
}

printf '%s\n' "${msg}" | awk '
    index($0, ":algo=NAME (:a=NAME)") { seen = 1 }
    seen && index($0, "auto      -> choose by") { found = 1; exit 0 }
    END { exit found ? 0 : 1 }
' || {
    echo "not ok" 1 - "medoids algo description lines are missing"
    exit 0
}

printf '%s\n' "${msg}" | awk '
    index($0, ":inittype=TYPE (:i=TYPE)") { seen = 1 }
    seen && index($0, "auto -> choose seed mode") { found = 1; exit 0 }
    END { exit found ? 0 : 1 }
' || {
    echo "not ok" 1 - "kmeans inittype description lines are missing"
    exit 0
}

echo "ok" 1 - "-H quantize choices use name -> description format"
exit 0
