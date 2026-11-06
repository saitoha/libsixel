#!/bin/sh
# Metal source bodies must disappear from non-Metal amalgamation builds.
# Policy: docs/build/autotools.md
# Policy: docs/testing/staticcheck.md
set -eux
echo '1..1'
set -v
awk '
function valid(text,    lines, n, i, line, depth, active, seen) {
    gsub(/\/\*([^*]|\*+[^*\/])*\*+\//, "", text)
    n = split(text, lines, "\n")
    depth = 0
    active[0] = 0
    seen = 0
    for (i = 1; i <= n; i++) {
        line = lines[i]
        sub(/^[ \t]+/, "", line)
        sub(/[ \t]+$/, "", line)
        if (line ~ /^#[ \t]*if(n?def)?[ \t]/) {
            depth++
            active[depth] = active[depth - 1]
            if (line ~ /^#if defined\(HAVE_METAL\)$/ ||
                    line ~ /^#ifdef HAVE_METAL$/) active[depth] = 1
        } else if (line ~ /^#[ \t]*(else|elif)([ \t]|$)/) {
            active[depth] = active[depth - 1]
        } else if (line ~ /^#[ \t]*endif([ \t]|$)/) {
            if (depth == 0) return 0
            depth--
        } else if (line ~ /^#import <(Foundation|Metal)\//) {
            if (!active[depth]) return 0
            seen = 1
        } else if (seen && line != "" && line !~ /^#/ &&
                line !~ /^\/\// && !active[depth]) return 0
    }
    return seen && depth == 0
}
function check_unit() {
    if (!valid(unit)) {
        print "unguarded Metal source: " name > "/dev/stderr"
        bad = 1
    }
}
BEGIN {
    body = "#import <Metal/Metal.h>\nint metal_body;\n"
    guarded = "#if defined(HAVE_METAL)\n" body "#endif\n"
    nested = "#if defined(HAVE_METAL)\n#if DEBUG\n" body \
        "#endif\n#endif\n"
    wrong_branch = "#if defined(HAVE_METAL)\n#else\n" body "#endif\n"
    premature = "#if defined(HAVE_METAL)\n#import <Metal/Metal.h>\n" \
        "#endif\nint metal_body;\n"
    if (!valid(guarded) || !valid(nested) || valid(body) ||
            valid(wrong_branch) || valid(premature)) exit 1
}
FNR == 1 {
    if (NR > 1) check_unit()
    name = FILENAME
    unit = ""
}
{ unit = unit $0 "\n" }
END { if (NR) check_unit(); exit bad || NR == 0 }
' "$1"/src/*metal.m || {
    echo 'not ok 1 - Metal bodies lack their amalgamation feature guard'
    exit 0
}
echo 'ok 1 - Metal bodies retain their amalgamation feature guard'
exit 0
