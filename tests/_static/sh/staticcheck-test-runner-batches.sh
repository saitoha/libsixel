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
!/^    / { key = ""; next }
key == "" { next }
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
# Read the ordinary-build rules as GNU make sees them after Automake strips
# its conditional. The probe prints the per-object options without compiling.
flags=$(
    {
        printf '%s\n' 'srcdir = source' 'OBJEXT = o'
        sed '/^if !WANT_AMALGAMATED_TOOLS$/d; /^endif$/d' \
            "$1/tests/runner-batches.am"
        cat <<'MAKEFILE'
probe_batches = .runner-batches/test_runner-batch_color_gamut_0001.inc.o \
    .runner-batches/test_runner-batch_color_icc_0010.inc.o ordinary.o
.PHONY: probe $(probe_batches)
probe: $(probe_batches)
$(probe_batches):
	@printf '%s\n' "["$(test_runner_CPPFLAGS)"]"
MAKEFILE
    } | "${MAKE:-make}" --no-print-directory -s -j1 -f - probe
) || {
    echo 'not ok 1 - test runner batch rules could not be read'
    exit 0
}
test "$flags" = '[-Isource/color/gamut/]
[-Isource/color/icc/]
[]' || {
    printf '# batch object compiler flags:\n%s\n' "$flags"
    echo 'not ok 1 - each batch object searches its original test directory'
    exit 0
}
echo 'ok 1 - test runner batches have bounded membership and include paths'
exit 0
