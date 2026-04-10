#!/bin/sh
# Emit TAP for test runner source coverage in amalgamation define list.

set -eu

echo "1..1"

src_root=$1
makefile_am=$src_root/tests/Makefile.am

if test ! -f "$makefile_am"; then
    echo "ok 1 # SKIP missing tests/Makefile.am"
    exit 0
fi

if missing_defines=$(awk '
function remember_source(path, basename, macro, parts, part_count) {
    part_count = split(path, parts, "/")
    basename = parts[part_count]
    sub(/\.c$/, "", basename)
    macro = "BUILD_TEST_" toupper(basename)
    gsub(/[^A-Z0-9]/, "_", macro)
    sources[macro] = path
}
function scan_sources(line, token) {
    while (match(line, /[A-Za-z0-9_.\/-]*[0-9]{4}_[A-Za-z0-9_.-]+\.c/)) {
        token = substr(line, RSTART, RLENGTH)
        remember_source(token)
        line = substr(line, RSTART + RLENGTH)
    }
}
function scan_defines(line, token) {
    while (match(line, /-DBUILD_TEST_[A-Z0-9_]+/)) {
        token = substr(line, RSTART + 2, RLENGTH - 2)
        defines[token] = 1
        line = substr(line, RSTART + RLENGTH)
    }
}
{
    if ($0 ~ /^[[:space:]]*test_runner_SOURCES[[:space:]]*=/) {
        in_sources = 1
    }
    if ($0 ~ /^[[:space:]]*TEST_RUNNER_AMALGAMATION_DEFINES[[:space:]]*=/) {
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
