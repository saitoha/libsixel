#!/bin/sh
# Each compiled wrapper owns one bounded batch of cases from one directory.
set -eux
echo '1..1'
set -v
awk '
/^runner_batches =/ { key = "wrappers"; next }
/^batch_[[:alnum:]_]+ =/ {
    key = $1
    if (key in count) bad = 1
    count[key] = 0
    next
}
!/^    / { next }
{
    path = $1
    if (key == "wrappers") {
        sub(/^\.runner-batches\//, "", path)
        sub(/\.inc\.c$/, "", path)
        if (listed[path]++) bad = 1
        wrappers++
    } else {
        dir = path
        sub(/\/[^\/]*$/, "", dir)
        if (path !~ /\/[0-9][0-9][0-9][0-9]_[^\/]+\.c$/ ||
            owned[path]++ || (count[key] && dir != category)) bad = 1
        category = dir
        count[key]++
    }
    if (bad) { print "invalid batch entry: " $0; exit 1 }
}
END {
    for (key in listed)
        if (count[key] < 2 || count[key] > 16) {
            print key " has " (count[key] + 0) " members"; bad = 1
        }
    for (key in count)
        if (!(key in listed)) { print "unregistered batch: " key; bad = 1 }
    exit bad || !wrappers
}' "$1/tests/runner-batches.am" >&2 || {
    echo 'not ok 1 - test runner batch membership'
    exit 0
}
echo 'ok 1 - test runner batch membership'
exit 0
