#!/bin/sh
# Policy: docs/build/autotools.md
# Policy: docs/build/test-harness.md
# Policy: docs/testing/staticcheck.md
# Keep the unbounded amalgamation macro inventory out of compiler argv.
set -eux
echo '1..1'
set -v
src_root=$1
# Avoid tracing the complete generated inventory into the test log.
set +xv
header=$(awk -f "$src_root/build-aux/test-runner-amalgamation-defines.awk" \
    "$src_root/tests/Makefile.am") || {
    echo 'not ok 1 - amalgamation macro header generation failed'
    exit 0
}
printf '%s\n' "$header" | awk '
FILENAME != "-" {
    if ($0 ~ /^TEST_RUNNER_AMALGAMATION_DEFINES[[:space:]]*=/) active = 1
    if (active) {
        for (i = 1; i <= NF; ++i)
            if ($i ~ /^-DBUILD_[A-Z0-9_]+$/) expected[substr($i, 3)] = 1
        if ($0 !~ /\\[[:space:]]*$/) active = 0
    }
    # No recipe or compiler flag may expand the entire inventory.
    if ($0 ~ /\$[({]TEST_RUNNER_AMALGAMATION_DEFINES[)}]/) bad = 1
    if ($0 ~ /^nodist_test_runner_SOURCES = test-runner-amalgamation.c/) source = 1
    if ($0 ~ /#include "test-runner-amalgamation-defines.h"/) include = FNR
    if ($0 ~ /#include "\.\.\/amalgamation\/sixel.c"/) implementation = FNR
    if ($0 ~ /^test_runner-test-runner-amalgamation.\$\(OBJEXT\):/) dependency = 1
    if ($0 ~ /^    test-runner-amalgamation-defines.h \$\(AMALGAMATION_SIXEL\)$/)
        inputs = 1
    next
}
/^#define BUILD_[A-Z0-9_]+ 1$/ {
    if (!($2 in expected) || seen[$2]++) bad = 1
    next
}
{ bad = 1 }
END {
    for (name in expected) if (!seen[name]) bad = 1
    exit bad || !source || !include || !dependency || !inputs || implementation <= include
}' "$src_root/tests/Makefile.am" - || {
    echo 'not ok 1 - amalgamation macro inventory is not file based'
    exit 0
}
echo 'ok 1 - amalgamation macro inventory is file based and complete'
exit 0
