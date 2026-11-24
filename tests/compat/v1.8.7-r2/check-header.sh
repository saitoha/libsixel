#!/bin/sh
# Compare only names published by the pinned release. New APIs stay free to
# evolve. Compile the same consumer against each installed header so that C,
# rather than a textual diff, evaluates constants and checks declarations.
set -eu
old=$1
current=$2
work=$3
: "${CC:=cc}"

awk '
    BEGIN { print "#include <stdio.h>\n#include <sixel.h>\nint main(void) {" }
    /^#define SIXEL_/ && $2 ~ /^SIXEL_[A-Z_0-9]+$/ {
        if ($2 == "SIXEL_ALLOCATE_BYTES_MAX") next
        print "printf(\"" $2 "=%lu\\n\", (unsigned long)(" $2 "));"
        macros++
    }
    /^enum [A-Za-z]+ \{/ {
        t = "enum " $2
        print "printf(\"" t "=%zu,%zu,%d\\n\", sizeof(" t "), _Alignof(" t "),"
        print "((" t ")-1 < (" t ")0));"
        enums++
    }
    /^[[:space:]]+[A-Z][A-Z_0-9]+[[:space:]]*=/ {
        print "printf(\"" $1 "=%d\\n\", (int)" $1 ");"
        aliases++
    }
    END {
        # The function-like status predicates include successful interruption.
        print "return !SIXEL_SUCCEEDED(SIXEL_OK) || SIXEL_FAILED(SIXEL_OK)"
        print " || !SIXEL_SUCCEEDED(SIXEL_INTERRUPTED)"
        print " || SIXEL_FAILED(SIXEL_INTERRUPTED)"
        print " || SIXEL_SUCCEEDED(SIXEL_BAD_ARGUMENT)"
        print " || !SIXEL_FAILED(SIXEL_BAD_ARGUMENT); }"
        if (!macros || !enums || !aliases) exit 1
    }
' "$old" > "$work/values.c"

# Check presence before redeclaring. A removed function would otherwise be
# silently declared again by the release text. The three sixel_bad_* hooks
# live under HAVE_TESTS and were never part of the installed public API.
awk '
    BEGIN { print "#include <sixel.h>" }
    /^SIXELAPI / { public_name = 1; next }
    public_name {
        name = $0
        sub(/\(.*/, "", name)
        if (name !~ /^sixel_[a-z0-9_]+$/) exit 1
        print "_Static_assert(sizeof(&" name ") > 0, \"" name "\");"
        public_name = 0
        count++
    }
    /^typedef / {
        if (match($0, /\(\* *sixel_[a-z0-9_]+\)/)) {
            name = substr($0, RSTART, RLENGTH)
            gsub(/[()* ]/, "", name)
        } else {
            name = $NF
            sub(/;$/, "", name)
        }
        print "_Static_assert(sizeof(" name " *) > 0, \"" name "\");"
    }
    END { if (count != 81) exit 1 }
' "$old" > "$work/declarations.c"

# Preprocessing strips multiline comments, build-only declarations and C++
# guards. The release starts its own declarations after <stddef.h> with
# sixel_index_t. C11 allows matching typedefs and function redeclarations,
# and requires a diagnostic for incompatible callbacks or signatures.
"$CC" -std=c11 -E -P -x c "$old" > "$work/release.i"
awk '
    /^typedef unsigned char sixel_index_t;/ { api = 1 }
    /^enum [A-Za-z]+ \{/ { inside_enum = 1 }
    inside_enum && /^};/ { inside_enum = 0; next }
    api && !inside_enum { print }
    END { if (!api) exit 1 }
' "$work/release.i" >> "$work/declarations.c"

"$CC" -std=c11 -Werror -Wno-deprecated-declarations -I"${current%/*}" \
    -fsyntax-only "$work/declarations.c"

"$CC" -std=c11 -I"${current%/*}" -x c -fsyntax-only - <<'EOF_FLOAT'
#include <sixel.h>
#if defined(SIXEL_PIXELFORMAT_LINEARRGBFLOAT32) || \
    defined(SIXEL_PIXELFORMAT_OKLABFLOAT32) || \
    defined(SIXEL_PIXELFORMAT_CIELABFLOAT32) || \
    defined(SIXEL_PIXELFORMAT_DIN99DFLOAT32)
# error Retired working representations must stay outside the public header.
#endif
_Static_assert(SIXEL_PIXELFORMAT_IS_FLOAT32(SIXEL_PIXELFORMAT_RGBFLOAT32),
               "one public float storage layout");
_Static_assert(!SIXEL_PIXELFORMAT_IS_FLOAT32(0x21),
               "private working code is not a public layout");
EOF_FLOAT

# The allocation limit is a measured compatibility difference. Check each
# value independently instead of allowing that macro to disappear from CI.
for version in old current; do
    case $version in
        old) dir=${old%/*}; limit=1343225856UL ;;
        current) dir=${current%/*}; limit=134217728UL ;;
    esac
    "$CC" -std=c11 -I"$dir" -x c -fsyntax-only - <<EOF_LIMIT
#include <sixel.h>
_Static_assert((SIXEL_ALLOCATE_BYTES_MAX) == $limit, "allocation limit");
EOF_LIMIT
    "$CC" -std=c11 -I"$dir" "$work/values.c" -o "$work/values"
    "$work/values" > "$work/$version-values"
done
diff -u "$work/old-values" "$work/current-values"
printf '%s\n' 'PASS release C declarations, numeric constants and enum layouts'
