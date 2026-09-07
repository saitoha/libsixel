#!/bin/sh
# Verify reciprocal links between enforced policy documents and owning tests.
# Policy: docs/AGENTS.md
# Policy: docs/testing/guide.md

set -eu

echo "1..1"

src_root=$(CDPATH='' cd -- "$1" && pwd -P)
tmpdir=$(mktemp -d "${TMPDIR:-/tmp}/libsixel-doc-test-links-XXXXXX")

# shellcheck disable=SC2329
cleanup() {
    rm -rf "$tmpdir"
}
trap cleanup EXIT HUP INT TERM

docs_root="$src_root/docs"
tests_root="$src_root/tests"
doc_links="$tmpdir/doc-links"
test_links="$tmpdir/test-links"
doc_pairs="$tmpdir/doc-pairs"
test_pairs="$tmpdir/test-pairs"
marked_docs="$tmpdir/marked-docs"
linked_docs="$tmpdir/linked-docs"
empty_docs="$tmpdir/empty-docs"
missing_backlinks="$tmpdir/missing-backlinks"
missing_doc_links="$tmpdir/missing-doc-links"
errors="$tmpdir/errors"

: > "$doc_links"
: > "$test_links"
: > "$marked_docs"
: > "$errors"

find "$docs_root" -type f -name '*.md' -exec \
    awk -v src_root="$src_root/" \
    -v marked_docs="$marked_docs" -v doc_links="$doc_links" \
    -v errors="$errors" '
    FNR == 1 {
        enforced = 0
        coverage_heading = 0
        delete coverage_ids
    }
    $0 == "## Test coverage" {
        coverage_heading = 1
    }
    $0 == "<!-- test-coverage: enforced -->" {
        if (coverage_heading == 0) {
            print FILENAME \
                ": enforced marker must follow a Test coverage heading" \
                >> errors
        }
        enforced = 1
        doc = substr(FILENAME, length(src_root) + 1)
        print doc >> marked_docs
        next
    }
    enforced != 0 {
        doc = substr(FILENAME, length(src_root) + 1)
        line = $0
        coverage_row = 0
        row_link_count = 0
        if (line ~ /^\| [^|]+ \|/ &&
            line !~ /^\| (ID|---) \|/) {
            coverage_row = 1
            split(line, fields, "|")
            coverage_id = fields[2]
            gsub(/^[[:space:]]+|[[:space:]]+$/, "", coverage_id)
            if (coverage_id !~ /^[A-Z][A-Z0-9]*-[0-9]+$/) {
                print doc ": invalid coverage ID: " coverage_id >> errors
            } else if (coverage_ids[coverage_id] != 0) {
                print doc ": duplicate coverage ID: " coverage_id >> errors
            }
            coverage_ids[coverage_id] = 1
        }
        while (match(line,
                     /\[tests\/[A-Za-z0-9_.\/-]+\]\([^()[:space:]]+\)/)) {
            token = substr(line, RSTART, RLENGTH)
            separator = index(token, "](")
            test_path = substr(token, 2, separator - 2)
            target = substr(token, separator + 2,
                            length(token) - separator - 2)
            print doc "|" test_path "|" target >> doc_links
            row_link_count += 1
            line = substr(line, RSTART + RLENGTH)
        }
        if (coverage_row != 0 && row_link_count == 0) {
            print doc ": coverage row has no owning test: " coverage_id \
                >> errors
        }
    }
' {} +

awk -F '|' '{ print $1 }' "$doc_links" | sort -u > "$linked_docs"
sort -u "$marked_docs" > "$marked_docs.sorted"
comm -23 "$marked_docs.sorted" "$linked_docs" > "$empty_docs"
while IFS= read -r doc_rel; do
    test -n "$doc_rel" || continue
    printf '%s: enforced document has no test links\n' \
        "$doc_rel" >> "$errors"
done < "$empty_docs"

while IFS='|' read -r doc_rel test_rel target; do
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
                     /Policy:[[:space:]]+docs\/[A-Za-z0-9_.\/-]+\.md/)) {
            if (FNR > 20 ||
                line !~ /^[[:space:]]*(#|\/\/|\*)[[:space:]]*Policy:/) {
                print test_path ": Policy reference must be a source " \
                    "comment within the first 20 lines" >> errors
            }
            policy = substr(line, RSTART, RLENGTH)
            sub(/^Policy:[[:space:]]+/, "", policy)
            print policy "|" test_path >> test_links
            line = substr(line, RSTART + RLENGTH)
        }
    }
' {} +

awk -F '|' '{ print $1 "|" $2 }' "$doc_links" | sort -u > "$doc_pairs"
sort -u "$test_links" > "$test_pairs"
comm -23 "$doc_pairs" "$test_pairs" > "$missing_backlinks"
comm -13 "$doc_pairs" "$test_pairs" > "$missing_doc_links"

while IFS='|' read -r doc_rel test_rel; do
    test -n "$doc_rel" || continue
    printf '%s: missing reciprocal Policy reference in %s\n' \
        "$doc_rel" "$test_rel" >> "$errors"
done < "$missing_backlinks"

while IFS='|' read -r doc_rel test_rel; do
    test -n "$doc_rel" || continue
    printf '%s: Policy reference is missing its document link to %s\n' \
        "$test_rel" "$doc_rel" >> "$errors"
done < "$missing_doc_links"

test ! -s "$errors" || {
    echo "not ok 1 - policy document and test links are reciprocal"
    sed 's/^/# /' "$errors"
    exit 1
}

echo "ok 1 - policy document and test links are reciprocal"
exit 0
