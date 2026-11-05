#!/bin/sh
# Shared test headers must survive repeated inclusion by unity sources.
# Policy: docs/build/test-harness.md
# Policy: docs/testing/staticcheck.md
set -eux
echo '1..1'
set -v
sh "$1/tests/_static/sh/list-source-files.sh" "$1/tests/_support" \
    --name '*common.h' --exec awk '
function guarded(text,    lines, n, i, name) {
    gsub(/\/\*([^*]|\*+[^*\/])*\*+\//, "", text)
    sub(/[[:space:]]+$/, "", text)
    n = split(text, lines, "\n")
    for (i = 1; i <= n; ++i) {
        if (lines[i] ~ /^#ifndef [A-Z_0-9]+$/) {
            name = substr(lines[i], 9)
            return lines[i + 1] == "#define " name &&
                text ~ /#endif[^\n]*\n*$/
        }
    }
    return 0
}
function finish() {
    if (file != "" && !guarded(text)) {
        print "# missing common-header include guard: " file
        failed = 1
    }
}
BEGIN {
    good = "#ifndef TEST_H\n#define TEST_H\nint x;\n#endif\n"
    if (!guarded(good) || guarded("int x;\n") ||
        guarded("#ifndef TEST_H\n#define OTHER_H\n#endif\n")) failed = 1
}
FNR == 1 { finish(); file = FILENAME; text = "" }
{ text = text $0 "\n" }
END { finish(); exit failed }
' || {
    echo 'not ok 1 - shared test headers are safe for repeated inclusion'
    exit 0
}
echo 'ok 1 - shared test headers are safe for repeated inclusion'
