#!/bin/sh
# Policy: docs/build/support-scripts.md
# Count decoded bytes across adjacent C literals without a target compiler.
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
function bounded(text,    n, lines, i, line, last, bytes) {
    chunks = 0
    n = split(text, lines, "\n")
    for (i = 1; i <= n; ++i) {
        line = lines[i]
        if (line == "" || line ~ /^\/\*.*\*\/$/) continue
        if (line !~ /^"([^"\\]|\\[\\"?tnr])*",?$/) return 0
        last = line ~ /,$/
        sub(/^"/, "", line)
        sub(/",?$/, "", line)
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
    if (!bounded(limit ",") || bounded(limit "\n\"x\",") ||
        !bounded("\"\\\"\\\\\\?\\n\",") || bounded("\"\\x41\",") ||
        bounded("\"unterminated\"")) {
        print "# literal byte-count controls failed"
        failed = 1
    }
}
{ source = source $0 "\n" }
END { exit failed || !bounded(source) || chunks < 2 }
' || {
    echo 'not ok 1 - embedded Metal literals violate the 1024-byte bound'
    exit 0
}
echo 'ok 1 - embedded Metal literals fit the 1024-byte bound'
exit 0
