#!/bin/sh
# Platform headers must protect their own system imports before hoisting.
# Policy: docs/build/autotools.md
# Policy: docs/testing/staticcheck.md
set -eux
echo '1..1'
set -v
awk '
function windows_condition(line) {
    if (line !~ /^#[ \t]*(if|elif)[ \t]+/) return 0
    sub(/^#[ \t]*(if|elif)[ \t]+/, "", line)
    gsub(/[ \t]/, "", line)
    # Accept a positive platform selector, optionally narrowed by AND.
    return line ~ /^defined\((_WIN32|_MSC_VER|__CYGWIN__|__MSYS__)\)(&&!?defined\([A-Za-z_][A-Za-z0-9_]*\))*$/ ||
        line == "defined(__CYGWIN__)||defined(__MSYS__)"
}
function valid(text,    n, lines, i, line, depth, win, metal) {
    gsub(/\/\*([^*]|\*+[^*\/])*\*+\//, "", text)
    gsub(/\\\n/, "", text)
    n = split(text, lines, "\n")
    depth = 0
    win[0] = metal[0] = 0
    for (i = 1; i <= n; ++i) {
        line = lines[i]
        sub(/\/\/.*$/, "", line)
        sub(/^[ \t]+/, "", line)
        sub(/[ \t]+$/, "", line)
        if (line ~ /^#[ \t]*if(n?def)?[ \t]/) {
            ++depth
            win[depth] = win[depth - 1]
            metal[depth] = metal[depth - 1]
        } else if (line ~ /^#[ \t]*(else|elif)([ \t]|$)/) {
            if (depth == 0) return 0
            win[depth] = win[depth - 1]
            metal[depth] = metal[depth - 1]
        } else if (line ~ /^#[ \t]*endif([ \t]|$)/) {
            if (depth == 0) return 0
            --depth
        }
        if (windows_condition(line) ||
            line ~ /^#[ \t]*ifdef[ \t]+(_WIN32|_MSC_VER|__CYGWIN__|__MSYS__)$/)
            win[depth] = 1
        if (line ~ /^#[ \t]*(if|elif) defined\(HAVE_METAL\)$/ ||
            line ~ /^#[ \t]*ifdef HAVE_METAL$/) metal[depth] = 1
        if (line ~ /^#[ \t]*(include|import)[ \t]*[<"](windows|winternl)\.h[>"]/ &&
            !win[depth]) return 0
        if (line ~ /^#[ \t]*(include|import)[ \t]*[<"](Metal|Foundation)\// &&
            !metal[depth]) return 0
    }
    return depth == 0
}
function check_unit(    unit, line, result) {
    if (!candidate) return
    # Only materialize headers with relevant imports; large tables and stb
    # headers otherwise cause quadratic string concatenation in some awks.
    unit = ""
    while ((result = (getline line < name)) > 0) unit = unit line "\n"
    close(name)
    if (result < 0 || !valid(unit)) {
        print "unguarded platform system header or unbalanced condition: " name
        bad = 1
    }
}
BEGIN {
    body = "#include <windows.h>\n#include <winternl.h>\n"
    guarded = "#if defined(__CYGWIN__) || defined(__MSYS__)\n" body "#endif\n"
    nested = "#ifdef _WIN32\n#if DEBUG\n" body "#endif\n#endif\n"
    alternate = "#if defined(__GNUC__)\n#elif defined(_MSC_VER)\n" body "#endif\n"
    wrong = "#ifdef _WIN32\n#else\n" body "#endif\n"
    broad = "#if defined(_WIN32) || DEBUG\n" body "#endif\n"
    ternary = "#if defined(_WIN32) && DEBUG ? 1 : 1\n" body "#endif\n"
    negated = "#if !defined(_WIN32)\n" body "#endif\n"
    metal = "#import <Metal/Metal.h>\n"
    if (!valid(guarded) || !valid(nested) || !valid(alternate) ||
        valid(body) || valid(wrong) || valid(broad) || valid(ternary) ||
        valid(negated) ||
        valid(metal) || !valid("#ifdef HAVE_METAL\n" metal "#endif\n")) bad = 1
}
FNR == 1 {
    if (NR > 1) check_unit()
    name = FILENAME
    candidate = 0
}
/#[ \t]*(include|import)[ \t]*[<"]((windows|winternl)\.h[>"]|(Metal|Foundation)\/)/ { candidate = 1 }
END { if (NR) check_unit(); exit bad || NR == 0 }
' "$1"/include/*.h "$1"/src/*.h "$1"/converters/*.h >&2 || {
    echo 'not ok 1 - platform headers protect their system imports'
    exit 0
}
echo 'ok 1 - platform headers protect their system imports'
exit 0
