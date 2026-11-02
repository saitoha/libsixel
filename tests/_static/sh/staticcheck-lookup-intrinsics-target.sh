#!/bin/sh
# Policy: docs/testing/staticcheck.md
# Policy: docs/misc/platforms/cosmopolitan.md
# Coverage: COSMO-02
# A shared configure result cannot identify the active fat-binary target.
set -eux
echo '1..1'
set -v
src_root=$1
awk '
function target(expression, name) {
    if (expression ~ /!/) return 0
    if (expression ~ /defined\(__SSE2__\)/ ||
        expression ~ /defined\(_M_X64\)/ ||
        expression ~ /defined\(_M_AMD64\)/) return 1
    for (name in aliases)
        if (expression == "defined(" name ")") return 1
    return 0
}
FNR == 1 {
    for (name in aliases) delete aliases[name]
    depth = 0
    guarded[0] = 0
    pending = ""
}
{
    line = pending $0
    if (sub(/\\[[:space:]]*$/, "", line)) {
        pending = line
        next
    }
    pending = ""
    sub(/^[[:space:]]*#[[:space:]]*/, "#", line)
    if (line ~ /^#if[[:space:]]/) {
        sub(/^#if[[:space:]]+/, "", line)
        depth++
        guarded[depth] = guarded[depth - 1] || target(line)
    } else if (line ~ /^#if(n?def)[[:space:]]/) {
        depth++
        guarded[depth] = guarded[depth - 1]
    } else if (line ~ /^#elif[[:space:]]/) {
        sub(/^#elif[[:space:]]+/, "", line)
        guarded[depth] = guarded[depth - 1] || target(line)
    } else if (line ~ /^#else([[:space:]]|$)/) {
        guarded[depth] = guarded[depth - 1]
    } else if (line ~ /^#endif([[:space:]]|$)/) {
        depth--
    } else if (line ~ /^#define[[:space:]]/ && guarded[depth]) {
        split(line, fields, /[[:space:]]+/)
        if (fields[3] == "1") aliases[fields[2]] = 1
    } else if (line ~ /^#include[[:space:]]+<emmintrin.h>/) {
        count++
        if (!guarded[depth]) {
            print FILENAME ":" FNR ": SSE2 header lacks compiler target guard"
            failed = 1
        }
    }
}
END { exit failed || count == 0 }
' "$src_root"/src/lookup-*.c >&2 || {
    echo 'not ok 1 - lookup intrinsics require the active compiler target'
    exit 0
}
echo 'ok 1 - lookup intrinsics require the active compiler target'
exit 0
