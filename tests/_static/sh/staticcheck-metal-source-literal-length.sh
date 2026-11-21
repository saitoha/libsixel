#!/bin/sh
# Policy: docs/build/support-scripts.md
# Check grouping and decoded byte counts without a target compiler.
set -eux
echo '1..1'
set -v
src_root=$1
generated=$(LC_ALL=C awk -f "$src_root/tools/embed-metal.awk" \
    "$src_root/src/lookup-policy-blockcull-gpu.metal") || {
    echo 'not ok 1 - Metal literal generation failed'
    exit 0
}
printf '%s\n' "$generated" | LC_ALL=C awk '
function bounded(text,    n, lines, i, line, first, last, bytes) {
    chunks = 0
    n = split(text, lines, "\n")
    for (i = 1; i <= n; ++i) {
        line = lines[i]
        if (line == "" || line ~ /^\/\*.*\*\/$/) continue
        first = sub(/^\(/, "", line)
        last = sub(/\),$/, "", line)
        if (first != (bytes == 0)) return 0
        if (line !~ /^"([^"\\]|\\[\\"?tnr])*"$/) return 0
        sub(/^"/, "", line)
        sub(/"$/, "", line)
        gsub(/\\[\\"?tnr]/, "x", line)
        bytes += length(line)
        if (bytes > 1024) return 0
        if (last) {
            ++chunks
            bytes = 0
        }
    }
    return chunks > 0 && bytes == 0
}
BEGIN {
    limit = "\"" sprintf("%1024s", "") "\""
    if (!bounded("(" limit "),") || bounded("(" limit "\n\"x\"),") ||
        !bounded("(\"\\\"\\\\\\?\\n\"),") || bounded("(\"\\x41\"),") ||
        bounded("(\"unterminated\"") || bounded(limit ",")) {
        print "# literal byte-count controls failed"
        failed = 1
    }
}
{ source = source $0 "\n" }
END { exit failed || !bounded(source) || chunks < 2 }
' || {
    echo 'not ok 1 - Metal chunks must be parenthesized and at most 1024 bytes'
    exit 0
}
# Count adjacent host string literals, ignoring whitespace and C comments.
# Metal source composed at runtime is bounded per array element.
LC_ALL=C awk '
FNR == 1 { bytes = 0; comment = 0; reported = 0 }
{
    line = $0
    if (comment) {
        if (!sub(/^.*\*\//, "", line)) next
        comment = 0
    }
    while (1) {
        sub(/^[[:space:]]+/, "", line)
        if (line == "" || line ~ /^\/\//) break
        if (line ~ /^\/\*/) {
            if (sub(/^\/\*.*\*\//, "", line)) continue
            comment = 1
            break
        }
        if (!match(line, /^@?"([^"\\]|\\.)*"/)) {
            bytes = 0
            break
        }
        literal = substr(line, 1, RLENGTH)
        line = substr(line, RLENGTH + 1)
        sub(/^@?"/, "", literal)
        sub(/"$/, "", literal)
        gsub(/\\./, "x", literal)
        bytes += length(literal)
        if (bytes > 4095 && !reported) {
            reported = 1
            print "# " FILENAME ":" FNR ": adjacent literals exceed 4095 bytes"
            failed = 1
            next
        }
    }
}
END { exit failed }
' "$src_root"/src/*-metal.m || {
    echo 'not ok 1 - Metal host literals exceed the C99 length guarantee'
    exit 0
}
echo 'ok 1 - embedded Metal source uses bounded literals'
exit 0
