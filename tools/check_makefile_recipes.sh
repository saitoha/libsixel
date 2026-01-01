#!/bin/sh
# Reject Makefile recipes that are indented with spaces instead of tabs.
# This script can be run directly or via the repository's pre-commit hook.

set -eu

repo_root=$(git rev-parse --show-toplevel)
cd "$repo_root"

# When invoked without arguments, check staged Makefile.am/Makefile.in files.
if [ "$#" -gt 0 ]; then
    files="$*"
else
    files=$(git diff --cached --name-only --diff-filter=ACM | \
        grep -E '(^|/)(Makefile\.am|Makefile\.in)$' || true)
fi

if [ -z "$files" ]; then
    exit 0
fi

errors=0
for file in $files; do
    if [ ! -f "$file" ]; then
        continue
    fi
    awk '
    BEGIN {
        expect_recipe = 0;
        rule_continuation = 0;
        errors = 0;
    }
    {
        line = $0;

        # If we are inside a recipe block, enforce leading tabs.
        if (expect_recipe == 1) {
            if (line ~ /^[\t]/) {
                expect_recipe = 1;
            } else if (line ~ /^[ ]/) {
                printf("%s:%d: recipe line must start with a tab, not spaces\n", FILENAME, NR);
                errors = 1;
                expect_recipe = 1;
            } else if (line ~ /^[[:space:]]*$/) {
                expect_recipe = 0;
            } else if (line ~ /^[^[:space:]]/) {
                expect_recipe = 0;
            }
        }

        # Handle multi-line rule headers continued with a backslash.
        if (rule_continuation == 1) {
            if (line ~ /\\$/) {
                rule_continuation = 1;
                expect_recipe = 0;
            } else {
                rule_continuation = 0;
                expect_recipe = 1;
            }
        }

        # Detect the start of a rule header when not already in a recipe.
        if (expect_recipe == 0 && rule_continuation == 0) {
            if (line ~ /^[^[:space:]=][^=]*:[[:space:]]*(#.*)?$/) {
                if (line ~ /\\$/) {
                    rule_continuation = 1;
                    expect_recipe = 0;
                } else {
                    expect_recipe = 1;
                }
            }
        }
    }
    END {
        if (errors) {
            exit 1;
        }
    }
    ' "$file" || errors=1
    if [ $errors -ne 0 ]; then
        echo "Aborting: spaces detected where make recipes require tabs" >&2
        exit 1
    fi
    errors=0

done

exit 0
