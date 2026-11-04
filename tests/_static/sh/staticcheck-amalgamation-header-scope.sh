#!/bin/sh
# Test implementation headers retain every conditional inclusion site.
# Policy: docs/build/autotools.md
# Policy: docs/testing/staticcheck.md
set -eux
echo '1..1'
set -v
src_root=$1
fixture=$(mktemp -d "${TMPDIR:-/tmp}/sixel-header-scope-XXXXXX")
trap 'rm -rf "$fixture"' EXIT HUP INT TERM
# This tiny tree is required to exercise the production source transformer.
mkdir "$fixture/include" "$fixture/tests" "$fixture/src"
: > "$fixture/config.h"
: > "$fixture/include/sixel.h"
printf '%s\n' '#pragma once' '#include "data.h"' \
    'static int scope_helper(void) { return scope_data; }' \
    > "$fixture/tests/helper.h"
printf '%s\n' 'static int scope_data = 7;' > "$fixture/tests/data.h"
printf '%s\n' 'static int scope_orphan;' > "$fixture/tests/orphan.h"
printf '%s\n' '#include "helper.h"' 'int scope_first(void) { return scope_helper(); }' \
    > "$fixture/tests/0001_first.c"
printf '%s\n' '#include "helper.h"' 'int scope_second(void) { return scope_helper(); }' \
    > "$fixture/tests/0002_second.c"
sh "$src_root/tools/gen-amalgamation.sh" "$fixture/sixel.c" \
    "$fixture" "$fixture" >&2 || {
    echo 'not ok' '1 - conditional header fixture generation'
    exit 0
}
awk '
/^#if defined\(BUILD_TEST_000[12]_/ { owner = 1 }
/^#endif$/ { owner = 0 }
/^#define SIXEL_AMALGAMATION_USE_HEADER_[0-9]+ 1 \/\* tests\/helper.h \*\/$/ {
    helpers++; if (!owner) bad = 1
}
/^#define SIXEL_AMALGAMATION_USE_HEADER_[0-9]+ 1 \/\* tests\/data.h \*\/$/ {
    data++; if (!owner) bad = 1
}
/^#define SIXEL_AMALGAMATION_USE_HEADER_[0-9]+ 1 \/\* tests\/orphan.h \*\/$/ { bad = 1 }
/^#if defined\(SIXEL_AMALGAMATION_USE_HEADER_/ { guarded = 1 }
/^#endif \/\* defined\(SIXEL_AMALGAMATION_USE_HEADER_/ { guarded = 0 }
/^static int scope_/ { bodies++; if (!guarded) bad = 1 }
END { exit bad || helpers != 2 || data != 2 || bodies != 3 }
' "$fixture/sixel.c" || {
    echo 'not ok' '1 - header selection must follow each consuming unit'
    exit 0
}
echo 'ok' '1 - header selection follows each consuming unit'
exit 0
