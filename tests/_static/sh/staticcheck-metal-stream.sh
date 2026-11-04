#!/bin/sh
# A native image returning to GNV must not enter file publication in stream mode.
# Policy: docs/build/support-scripts.md
# Policy: docs/testing/staticcheck.md
set -eux
echo '1..1'
set -v
# shellcheck disable=SC2016
awk '
function terminal(text,    n, lines, i, state, line) {
    n = split(text, lines, "\n")
    for (i = 1; i <= n; ++i) {
        line = lines[i]
        gsub(/^[[:space:]]+|[[:space:]]+$/, "", line)
        if (line == "" || line ~ /^#/) continue
        if (state == 0 && line == "if test \"$output\" = -; then") {
            state = 1
        } else if (state == 1) {
            if (line != invoke) return 0
            state = 2
        } else if (state == 2) {
            if (line != "exit $?") return 0
            state = 3
        } else if (state == 3) {
            return line == "fi"
        }
    }
    return 0
}
BEGIN {
    invoke = "LC_ALL=C \"${AWK:-awk}\" -f \"${0%/*}/embed-metal.awk\" \"$source\""
    prefix = "if test \"$output\" = -; then\n"
    good = prefix invoke "\nexit $?\nfi"
    if (!terminal(good) || terminal(prefix invoke "\nfi") ||
        terminal(prefix invoke "\nexit 0\nfi") ||
        terminal(prefix "LC_ALL=C exec awk\nexit $?\nfi") ||
        terminal(prefix invoke "\nmv temporary output\nexit $?\nfi")) {
        print "# Metal stream controls failed"
        failed = 1
    }
}
{ source = source $0 "\n" }
END { exit failed || !terminal(source) }
' "$1/tools/embed-metal.sh" || {
    echo 'not ok 1 - Metal stream mode terminates with generator status'
    exit 0
}
echo 'ok 1 - Metal stream mode terminates with generator status'
exit 0
