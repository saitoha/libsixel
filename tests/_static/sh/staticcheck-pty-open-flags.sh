#!/bin/sh
# PTY creation accepts only the portable access/controlling-terminal flags.
# Policy: docs/testing/staticcheck.md
# Policy: docs/encoder/output/output-colorspace.md
set -eux
echo '1..1'
set -v
awk '
function invalid(text) {
    return text ~ /posix_openpt[[:space:]]*\([^)]*O_NONBLOCK/
}
BEGIN {
    if (!invalid("posix_openpt(O_RDWR | O_NOCTTY | O_NONBLOCK)") ||
        invalid("posix_openpt(O_RDWR | O_NOCTTY)")) exit 1
}
{
    text = text $0 "\n"
    if (FNR == 1 && NR != 1) text = $0 "\n"
    if (invalid(text)) {
        print "# nonportable posix_openpt flags: " FILENAME
        failed = 1
    }
    if ($0 ~ /;/) text = ""
}
END { exit failed }
' "$1"/tests/_support/cli/*.c || {
    echo 'not ok 1 - PTY allocation uses portable open flags'
    exit 0
}
echo 'ok 1 - PTY allocation uses portable open flags'
