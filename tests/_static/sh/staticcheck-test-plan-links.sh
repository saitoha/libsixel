#!/bin/sh
# Verify complete reciprocal links between test plans and covered tests.
# Policy: docs/AGENTS.md
# Policy: docs/testing/guide.md
# Policy: docs/testing/staticcheck.md

set -eu

echo "1..1"

src_root=$(CDPATH='' cd -- "$1" && pwd -P)
tmpdir=$(mktemp -d "${TMPDIR:-/tmp}/libsixel-test-plan-links-XXXXXX")

# shellcheck disable=SC2329
cleanup() {
    rm -rf "$tmpdir"
}
trap cleanup EXIT HUP INT TERM

docs_root="$src_root/docs"
tests_root="$src_root/tests"
plan_specs="$tmpdir/plan-specs"
expected_pairs="$tmpdir/expected-pairs"
doc_links="$tmpdir/doc-links"
doc_pairs="$tmpdir/doc-pairs"
doc_pairs_unique="$tmpdir/doc-pairs-unique"
test_links="$tmpdir/test-links"
test_pairs_unique="$tmpdir/test-pairs-unique"
duplicates="$tmpdir/duplicates"
missing="$tmpdir/missing"
extra="$tmpdir/extra"
errors="$tmpdir/errors"
generated_check="$tmpdir/generated-check"

: > "$plan_specs"
: > "$doc_links"
: > "$test_links"
: > "$errors"

find "$docs_root" -type f -name '*.md' -exec \
    awk -v src_root="$src_root/" -v plan_specs="$plan_specs" \
    -v doc_links="$doc_links" -v errors="$errors" '
    FNR == 1 {
        if (active != 0) {
            print active_doc ": missing <!-- test-plan-end -->" >> errors
        }
        active = 0
        active_doc = substr(FILENAME, length(src_root) + 1)
    }
    /^<!-- test-plan: tests\/[A-Za-z0-9_.\/-]+\/\*\.[A-Za-z0-9]+ -->$/ {
        if (active != 0) {
            print active_doc ": nested test-plan marker" >> errors
            next
        }
        plan = $0
        sub(/^<!-- test-plan: /, "", plan)
        sub(/ -->$/, "", plan)
        key = active_doc "|" plan
        if (seen[key] != 0) {
            print active_doc ": duplicate test-plan marker: " plan >> errors
        }
        seen[key] = 1
        print key >> plan_specs
        active = 1
        next
    }
    $0 == "<!-- test-plan-end -->" {
        if (active == 0) {
            print active_doc ": unmatched test-plan-end marker" >> errors
        }
        active = 0
        next
    }
    active != 0 {
        line = $0
        while (match(line,
                     /\[tests\/[A-Za-z0-9_.\/-]+\]\([^()[:space:]]+\)/)) {
            token = substr(line, RSTART, RLENGTH)
            separator = index(token, "](")
            test_path = substr(token, 2, separator - 2)
            target = substr(token, separator + 2,
                            length(token) - separator - 2)
            print active_doc "|" test_path "|" target >> doc_links
            line = substr(line, RSTART + RLENGTH)
        }
    }
    END {
        if (active != 0) {
            print active_doc ": missing <!-- test-plan-end -->" >> errors
        }
    }
' {} +

test -s "$plan_specs" || {
    printf '%s\n' "docs: no test-plan marker found" >> "$errors"
}

: > "$expected_pairs"
plan_index=0
while IFS='|' read -r doc_rel plan_glob; do
    test -n "$doc_rel" || continue
    plan_index=$((plan_index + 1))
    plan_dir=${plan_glob%/*}
    plan_name=${plan_glob##*/}
    matches="$tmpdir/matches-$plan_index"
    test -d "$src_root/$plan_dir" || {
        printf '%s: test-plan directory does not exist: %s\n' \
            "$doc_rel" "$plan_dir" >> "$errors"
        continue
    }
    find "$src_root/$plan_dir" -type f -name "$plan_name" | \
        LC_ALL=C sort | while IFS= read -r test_path; do
            test_rel=${test_path#"$src_root/"}
            test "${test_rel%/*}" = "$plan_dir" || continue
            printf '%s|%s\n' "$doc_rel" "$test_rel"
        done > "$matches"
    test -s "$matches" || {
        printf '%s: test-plan glob has no files: %s\n' \
            "$doc_rel" "$plan_glob" >> "$errors"
        continue
    }
    cat "$matches" >> "$expected_pairs"
done < "$plan_specs"
LC_ALL=C sort -u "$expected_pairs" -o "$expected_pairs"

while IFS='|' read -r doc_rel test_rel target; do
    test -n "$doc_rel" || continue
    test -f "$src_root/$test_rel" || {
        printf '%s: test label does not exist: %s\n' \
            "$doc_rel" "$test_rel" >> "$errors"
    }
    doc_dir=${doc_rel%/*}
    target_path="$src_root/$doc_dir/$target"
    test -f "$target_path" || {
        printf '%s: Markdown target does not exist: %s\n' \
            "$doc_rel" "$target" >> "$errors"
        continue
    }
    target_dir=${target_path%/*}
    target_base=${target_path##*/}
    resolved_target=$(CDPATH='' cd -- "$target_dir" && pwd -P)/$target_base
    test "$resolved_target" = "$src_root/$test_rel" || {
        printf '%s: test label %s resolves to a different target: %s\n' \
            "$doc_rel" "$test_rel" "$target" >> "$errors"
    }
done < "$doc_links"

awk -F '|' '{ print $1 "|" $2 }' "$doc_links" | \
    LC_ALL=C sort > "$doc_pairs"
uniq -d "$doc_pairs" > "$duplicates"
while IFS='|' read -r doc_rel test_rel; do
    test -n "$doc_rel" || continue
    printf '%s: duplicate test-plan link to %s\n' \
        "$doc_rel" "$test_rel" >> "$errors"
done < "$duplicates"
LC_ALL=C sort -u "$doc_pairs" > "$doc_pairs_unique"

find "$tests_root" -type f \
    \( -name '*.t' -o -name '*.c' -o -name '*.sh' -o \
       -name '*.py' -o -name '*.rb' -o -name '*.pl' -o \
       -name '*.php' \) \
    -exec awk -v src_root="$src_root/" -v test_links="$test_links" \
    -v errors="$errors" '
    {
        test_path = substr(FILENAME, length(src_root) + 1)
        line = $0
        while (match(line,
                     /Test-plan:[[:space:]]+docs\/[A-Za-z0-9_.\/-]+\.md/)) {
            if (FNR > 20 ||
                line !~ /^[[:space:]]*(#|\/\/|\*)[[:space:]]*Test-plan:/) {
                print test_path ": Test-plan reference must be a source " \
                    "comment within the first 20 lines" >> errors
            }
            plan = substr(line, RSTART, RLENGTH)
            sub(/^Test-plan:[[:space:]]+/, "", plan)
            print plan "|" test_path >> test_links
            line = substr(line, RSTART + RLENGTH)
        }
    }
' {} +

LC_ALL=C sort "$test_links" -o "$test_links"
uniq -d "$test_links" > "$duplicates"
while IFS='|' read -r doc_rel test_rel; do
    test -n "$doc_rel" || continue
    printf '%s: duplicate Test-plan reference to %s\n' \
        "$test_rel" "$doc_rel" >> "$errors"
done < "$duplicates"
LC_ALL=C sort -u "$test_links" > "$test_pairs_unique"

comm -23 "$expected_pairs" "$doc_pairs_unique" > "$missing"
while IFS='|' read -r doc_rel test_rel; do
    test -n "$doc_rel" || continue
    printf '%s: test-plan inventory is missing %s\n' \
        "$doc_rel" "$test_rel" >> "$errors"
done < "$missing"

comm -13 "$expected_pairs" "$doc_pairs_unique" > "$extra"
while IFS='|' read -r doc_rel test_rel; do
    test -n "$doc_rel" || continue
    printf '%s: test-plan inventory has an out-of-scope link to %s\n' \
        "$doc_rel" "$test_rel" >> "$errors"
done < "$extra"

comm -23 "$expected_pairs" "$test_pairs_unique" > "$missing"
while IFS='|' read -r doc_rel test_rel; do
    test -n "$doc_rel" || continue
    printf '%s: missing reciprocal Test-plan reference in %s\n' \
        "$doc_rel" "$test_rel" >> "$errors"
done < "$missing"

comm -13 "$expected_pairs" "$test_pairs_unique" > "$extra"
while IFS='|' read -r doc_rel test_rel; do
    test -n "$doc_rel" || continue
    printf '%s: Test-plan reference is outside the document glob in %s\n' \
        "$test_rel" "$doc_rel" >> "$errors"
done < "$extra"

python_bin=${PYTHON_STATICCHECK:-python3}
(CDPATH='' cd -- "$src_root" &&
    "$python_bin" tools/generate_builtin_loader_coverage.py --check) \
    >"$generated_check" 2>&1 || {
    printf '%s\n' \
        "docs/testing/builtin-loader-coverage.md: generated content is stale" \
        >> "$errors"
    sed 's/^/  /' "$generated_check" >> "$errors"
}

test ! -s "$errors" || {
    echo "not ok 1 - test-plan inventories and covered tests are reciprocal"
    sed 's/^/# /' "$errors"
    exit 1
}

echo "ok 1 - test-plan inventories and covered tests are reciprocal"
exit 0
