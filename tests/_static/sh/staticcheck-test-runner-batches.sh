#!/bin/sh
# Each compiled wrapper owns one bounded batch of cases from one directory.
set -eux
echo '1..1'
set -v
# Modern make silently accepts syntax that GNV Make 3.78.1 cannot expand.
awk '
/^[[:space:]]*#/ { next }
/\$[(]eval([[:space:]]|\\$)|\$[{]eval([[:space:]]|\\$)/ {
    printf "%s:%d: GNU Make eval requires version 3.80 or newer\n", FILENAME, FNR
    bad = 1
}
END { exit bad }
' "$1/tests/runner-batches.am" "$1/tests/Makefile.am" \
    "$1/tests/Makefile.in" >&2 || {
    echo 'not ok' 1 - 'test runner rules must work without GNU Make eval'
    exit 0
}

awk '
/^(@WANT_AMALGAMATED_TOOLS_FALSE@)?\.runner-batches\/batch_[[:alnum:]_]+\.inc\.c:/ {
    rule = $1
    sub(/^@WANT_AMALGAMATED_TOOLS_FALSE@/, "", rule)
    sub(/^\.runner-batches\//, "", rule)
    sub(/\.inc\.c:$/, "", rule)
    if (!(rule in listed) || NF != 2 || $2 != "$(" rule ")" ||
        prerequisites[FILENAME, rule]++) {
        printf "%s:%d: invalid batch prerequisite: %s\n", FILENAME, FNR, $0
        bad = 1
    }
    next
}
NR != FNR { next }
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
    for (key in listed)
        for (file = 1; file < ARGC; file++)
            if (!((ARGV[file], key) in prerequisites)) {
                print ARGV[file] ": missing wrapper prerequisite: " key
                bad = 1
            }
    exit bad || !wrappers
}' "$1/tests/runner-batches.am" "$1/tests/Makefile.in" >&2 || {
    echo 'not ok' 1 - 'test runner batch membership and prerequisites'
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
probe_batches = .runner-batches/test_runner-batch_encoder_indexed_0185.inc.o \
    .runner-batches/test_runner-batch_color_gamut_0001.inc.o \
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
test "$flags" = '[-Isource/encoder/indexed/]
[-Isource/color/gamut/]
[-Isource/color/icc/]
[]' || {
    printf '# batch object compiler flags:\n%s\n' "$flags"
    echo 'not ok 1 - each batch object searches its original test directory'
    exit 0
}
echo 'ok' 1 - 'test runner batches have bounded membership and include paths'
exit 0
