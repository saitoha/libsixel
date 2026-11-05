#!/bin/sh
# PTY creation accepts only the portable access/controlling-terminal flags.
# Policy: docs/testing/staticcheck.md
# Policy: docs/encoder/output/output-colorspace.md
set -eux
echo '1..1'
set -v
sh "$1/tests/_static/sh/list-source-files.sh" "$1/tests" \
    --name '*.[ch]' --exec awk '
function invalid(text) {
    return text ~ /posix_openpt[[:space:]]*\([^)]*O_NONBLOCK/
}
BEGIN {
    if (!invalid("posix_openpt(O_RDWR | O_NOCTTY | O_NONBLOCK)") ||
        invalid("posix_openpt(O_RDWR | O_NOCTTY)")) exit 1
}
FNR == 1 { text = "" }
text != "" || /posix_openpt/ {
    text = text $0 "\n"
    if (invalid(text)) {
        print "# nonportable posix_openpt flags: " FILENAME
        failed = 1
    }
    if ($0 ~ /\)/) text = ""
}
END { exit failed }
' || {
    echo 'not ok 1 - PTY allocation uses portable open flags'
    exit 0
}
echo 'ok 1 - PTY allocation uses portable open flags'
