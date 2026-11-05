#!/bin/sh
# Converter tests must skip before invoking a disabled optional command.
# Policy: docs/build/test-harness.md
# Policy: docs/testing/staticcheck.md
set -eux
echo '1..1'
set -v
sh "$1/tests/_static/sh/list-source-files.sh" "$1/tests" \
    --name '*.t' --exec awk '
function inspect(line,    i, tool) {
    if (line ~ /^[[:space:]]*(#|:)/) return
    for (i = 1; i <= 2; i++) {
        tool = tools[i]
        if (line ~ ("test.*HAVE_" tool "[^[:alnum:]_].*= 1") ||
            line ~ ("test -x.*" tool "_PATH")) guarded[i] = 1
        else if (index(line, tool "_PATH") && !guarded[i]) return 1
    }
    return 0
}
BEGIN {
    tools[1] = "IMG2SIXEL"; tools[2] = "SIXEL2PNG"
    if (!inspect("$IMG2SIXEL_PATH input") ||
        inspect("test ${HAVE_IMG2SIXEL-} = 1 || {" ) ||
        inspect("$IMG2SIXEL_PATH input")) failed = 1
}
FNR == 1 { guarded[1] = guarded[2] = 0 }
/IMG2SIXEL_PATH|SIXEL2PNG_PATH|HAVE_IMG2SIXEL|HAVE_SIXEL2PNG/ {
    if (!inspect($0)) next
    print "# " FILENAME ":" FNR ": converter used before capability guard"
    failed = 1
}
END { exit failed }
' || {
    echo 'not ok 1 - converter tests guard optional tools'
    exit 0
}
echo 'ok 1 - converter tests guard optional tools'
