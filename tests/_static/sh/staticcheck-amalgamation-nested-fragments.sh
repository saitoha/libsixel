#!/bin/sh
# Exercise include-site expansion of nested implementation fragments.
# Policy: docs/build/autotools.md
# Policy: docs/testing/staticcheck.md
set -eux
echo '1..1'
set -v
src_root=$1
fixture=$(mktemp -d "${TMPDIR:-/tmp}/sixel-nested-fragments-XXXXXX")
trap 'rm -rf "$fixture"' EXIT HUP INT TERM
# A minimal source tree exercises the generator without a compiler.
mkdir "$fixture/include" "$fixture/src" "$fixture/src/generated-metal"
: > "$fixture/config.h"
: > "$fixture/include/sixel.h"
printf '%s\n' 'typedef int nested_type;' '#if 0' \
    '#include "outer.inc.h"' '#endif' '#if ACTIVE' \
    '#include "outer.inc.h"' '#endif' > "$fixture/src/owner.c"
printf '%s\n' 'static nested_type nested_outer;' \
    '#include "inner.inc.h"' > "$fixture/src/outer.inc.h"
printf '%s\n' 'static nested_type nested_inner;' \
    'static char nested_shader[] = {' \
    '#include "generated-metal/nested.inc.h"' '};' \
    > "$fixture/src/inner.inc.h"
printf '%s\n' '0x61, 0x00 /* nested_shader_bytes */' \
    > "$fixture/src/generated-metal/nested.inc.h"
sh "$src_root/tools/gen-amalgamation.sh" "$fixture/sixel.c" \
    "$fixture" "$fixture" >&2 || {
    echo 'not ok 1 - nested fragment generation'
    exit 0
}
awk '
/^typedef int nested_type;/ { owner = 1 }
/^#if 0$/ { site = 1 }
/^#if ACTIVE$/ { site = 2 }
/^#define SIXEL_AMALGAMATION_HEADER_SRC_OUTER_INC_H 1$/ { outer_guard++ }
/^#define SIXEL_AMALGAMATION_HEADER_SRC_INNER_INC_H 1$/ { inner_guard++ }
/^static nested_type nested_outer;/ {
    if (!owner || site != outer + 1) bad = 1
    outer++
}
/^static nested_type nested_inner;/ {
    if (outer != inner + 1) bad = 1
    inner++
}
/nested_shader_bytes/ { if (inner != bytes + 1) bad = 1; bytes++ }
/^# *include.*(outer.inc.h|inner.inc.h|generated-metal)/ { bad = 1 }
END {
    exit bad || outer != 2 || inner != 2 || bytes != 2 ||
        outer_guard != 2 || inner_guard != 2
}
' "$fixture/sixel.c" || {
    echo 'not ok 1 - nested fragments must retain both conditional sites'
    exit 0
}
echo 'ok 1 - nested fragments retain conditional scope and shader bytes'
exit 0
