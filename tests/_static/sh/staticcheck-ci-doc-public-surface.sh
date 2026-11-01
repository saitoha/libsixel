#!/bin/sh
# Keep CI documentation limited to repository-visible sources and contracts.
# Policy: docs/ci/design.md

set -eu
source_files_helper=$(CDPATH='' cd -- "${0%/*}" && pwd -P)/list-source-files.sh

src_root=$1
matches=$(sh "$source_files_helper" "$src_root/docs/ci" --name '*.md' --exec awk \
 -v forbidden='libsixel-ci|`/?srv/|jobs[.]tsv|runner-profiles[.]tsv|workers[.]tsv|resource-classes[.]tsv|job-definition/' '
    $0 ~ forbidden {
        print FILENAME ":" FNR ":" $0
    }
')

echo "1..1"

test -z "$matches" || {
    echo "not ok 1 - CI docs expose private repository paths"
    printf '%s\n' "$matches" | sed 's/^/# /'
    exit 1
}

echo "ok 1 - CI docs describe only repository-visible sources and contracts"
exit 0
