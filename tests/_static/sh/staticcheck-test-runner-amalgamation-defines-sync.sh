#!/bin/sh
# Emit TAP for test runner source coverage in amalgamation define list.
# Policy: docs/testing/guide.md

set -eu

echo "1..1"

src_root=$1
makefile_am=$src_root/tests/Makefile.am

if test ! -f "$makefile_am"; then
    echo "ok 1 # SKIP missing tests/Makefile.am"
    exit 0
fi

if missing_defines=$(awk '
# Match gen-amalgamation.sh: numbered test basenames use BUILD_TEST_,
# other test/support basenames use BUILD_, and include fragments have no
# standalone guard. External always-included src units need no define.
function remember_source(path, basename, macro, parts, part_count, external) {
    external = sub(/^\$\(top_srcdir\)\//, "", path)
    if (path ~ /\.inc\.c$/) {
        return
    }
    if (external && path !~ /^(tests|gdk-pixbuf-loader)\// &&
        path !~ /^assessment\/(assessment|color)\.c$/ &&
        path !~ /^converters\/(cli|path|aborttrace|completion_utils|malloc_stub|compat|terminal-color|terminal-color-tty)\.c$/) {
        return
    }
    part_count = split(path, parts, "/")
    basename = parts[part_count]
    sub(/\.[cm]$/, "", basename)
    macro = toupper(basename)
    gsub(/[^A-Z0-9_]/, "_", macro)
    if (macro ~ /^[0-9]/) {
        macro = "TEST_" macro
    }
    sources["BUILD_" macro] = path
}
# Include test-runner convenience libraries with isolated compiler flags.
function scan_sources(line, token) {
    while (match(line, /(\$\(top_srcdir\)\/)?[A-Za-z0-9_.\/-]+\.[cm]([[:space:]]|$)/)) {
        token = substr(line, RSTART, RLENGTH)
        sub(/[[:space:]]+$/, "", token)
        remember_source(token)
        line = substr(line, RSTART + RLENGTH)
    }
}
function scan_defines(line, token) {
    while (match(line, /-DBUILD_[A-Z0-9_]+/)) {
        token = substr(line, RSTART + 2, RLENGTH - 2)
        defines[token] = 1
        line = substr(line, RSTART + RLENGTH)
    }
}
{
    if ($0 ~ /^[[:space:]]*(test_runner|[[:alnum:]_]+_la)_SOURCES[[:space:]]*\+?=/) {
        in_sources = 1
    }
    if ($0 ~ /^[[:space:]]*TEST_RUNNER_AMALGAMATION_DEFINES[[:space:]]*\+?=/) {
        in_defines = 1
    }

    if (in_sources) {
        scan_sources($0)
    }
    if (in_defines) {
        scan_defines($0)
    }

    if (in_sources && $0 !~ /\\[[:space:]]*$/) {
        in_sources = 0
    }
    if (in_defines && $0 !~ /\\[[:space:]]*$/) {
        in_defines = 0
    }
}
END {
    for (macro in sources) {
        if (!(macro in defines)) {
            printf "%s\t%s\n", macro, sources[macro]
            missing = 1
        }
    }
    if (missing == 1) {
        exit 1
    }
}
' "$makefile_am"); then
    :
else
    echo "not ok 1 - test runner amalgamation defines stay synchronized"
    printf '%s\n' "$missing_defines" |
        LC_ALL=C sort |
        sed 's/^/# missing amalgamation define: /'
    exit 1
fi

echo "ok 1 - test runner amalgamation defines stay synchronized"
