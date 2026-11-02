#!/bin/sh
# Verify complete reciprocal links between test plans and covered tests.

set -eu


src_root=$(CDPATH='' cd -- "$1" && pwd -P)
tmpdir=$(mktemp -d "${TMPDIR:-/tmp}/libsixel-test-plan-links-XXXXXX")

# shellcheck disable=SC2329
cleanup() {
    rm -rf "$tmpdir"
}
trap cleanup EXIT HUP INT TERM

docs_root="$src_root/docs"
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

: > "$plan_specs"
: > "$doc_links"
: > "$test_links"
: > "$errors"

sh "${0%/*}/list-source-files.sh" "$docs_root" --name '*.md' --exec \
    awk -v src_root="$src_root/" -v plan_specs="$plan_specs" \
    -v doc_links="$doc_links" -v errors="$errors" '
    FNR == 1 {
        if (active != 0) {
            print active_doc ": missing <!-- test-plan-end -->" >> errors
        }
        active = 0
        active_doc = substr(FILENAME, length(src_root) + 1)
    }
    /^<!-- test-plan: (tests\/[A-Za-z0-9_.\/-]+\/(\?\?\?\?_)?\*\.[A-Za-z0-9]+|@tests\/_support\/test-plans\/[A-Za-z0-9_-]+\.list) -->$/ {
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
    /^<!-- test-plan:/ {
        print active_doc ": unsupported test-plan marker: " $0 >> errors
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
'

test -s "$plan_specs" || {
    printf '%s\n' "docs: no test-plan marker found" >> "$errors"
}

: > "$expected_pairs"
plan_index=0
while IFS='|' read -r doc_rel plan_glob; do
    test -n "$doc_rel" || continue
    plan_index=$((plan_index + 1))
    matches="$tmpdir/matches-$plan_index"
    : > "$matches"
    case "$plan_glob" in
        @*)
            manifest=$src_root/${plan_glob#@}
            test -r "$manifest" || {
                printf '%s: missing manifest: %s\n' "$doc_rel" "$plan_glob" >> "$errors"
                continue
            }
            awk '
                $0 !~ /^tests\/[A-Za-z0-9_.\/-]+$/ || $0 ~ /(^|\/)\.\.(\/|$)/ {
                    print FILENAME ": invalid test path: " $0; next
                }
                seen[$0]++ { print FILENAME ": duplicate test path: " $0 }
            ' "$manifest" >> "$errors"
            while IFS= read -r test_rel; do
                test -f "$src_root/$test_rel" || printf '%s: missing scoped test: %s\n' "$doc_rel" "$test_rel" >> "$errors"
                printf '%s|%s\n' "$doc_rel" "$test_rel" >> "$matches"
            done < "$manifest"
            ;;
        *)
            plan_dir=${plan_glob%/*}
            plan_name=${plan_glob##*/}
            test -d "$src_root/$plan_dir" || {
                printf '%s: test-plan directory does not exist: %s\n' "$doc_rel" "$plan_dir" >> "$errors"
                continue
            }
            # The marker grammar allows only one basename glob, never code.
            # shellcheck disable=SC2231
            for test_path in "$src_root/$plan_dir"/$plan_name; do
                test -f "$test_path" || continue
                printf '%s|%s\n' "$doc_rel" "${test_path#"$src_root/"}" >> "$matches"
            done
            ;;
    esac
    test -s "$matches" || {
        printf '%s: test-plan glob has no files: %s\n' \
            "$doc_rel" "$plan_glob" >> "$errors"
        continue
    }
    cat "$matches" >> "$expected_pairs"
done < "$plan_specs"
LC_ALL=C sort -u "$expected_pairs" -o "$expected_pairs"

# Adjacent inventory entries usually share a directory. Cache only its
# physical resolution; existence and label equality are still checked per link.
last_target_dir=
resolved_dir=
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
    if test "$target_dir" != "$last_target_dir"; then
        resolved_dir=$(CDPATH='' cd -- "$target_dir" && pwd -P)
        last_target_dir=$target_dir
    fi
    resolved_target=$resolved_dir/$target_base
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
    printf '%s: duplicate inventory link: %s\n' \
        "$doc_rel" "$test_rel" >> "$errors"
done < "$duplicates"
LC_ALL=C sort -u "$doc_pairs" > "$doc_pairs_unique"

# Never sort the shared inventory in place: other workers may still read it.
reference_dir=${2:-${SIXEL_STATICCHECK_REFERENCE_DIR:-}}
if test -z "$reference_dir"; then
    reference_dir=$tmpdir
    sh "${0%/*}/collect-source-references.sh" \
        "$src_root" "$reference_dir" test-plan
fi
cat "$reference_dir/test-plan-errors" >> "$errors"
LC_ALL=C sort "$reference_dir/test-plan-links" > "$test_links"
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
    printf '%s: missing inventory link: %s\n' \
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
    printf '%s: missing Test-plan reference: %s\n' \
        "$doc_rel" "$test_rel" >> "$errors"
done < "$missing"

comm -13 "$expected_pairs" "$test_pairs_unique" > "$extra"
while IFS='|' read -r doc_rel test_rel; do
    test -n "$doc_rel" || continue
    printf '%s: Test-plan reference is outside the document glob in %s\n' \
        "$test_rel" "$doc_rel" >> "$errors"
done < "$extra"

test ! -s "$errors" || {
    cat "$errors" >&2
    exit 1
}
exit 0
