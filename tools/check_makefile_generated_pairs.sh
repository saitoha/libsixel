#!/bin/sh
# Reject commits that stage a Makefile.am without its generated Makefile.in.
#
# Comparing mtimes is not sufficient: config.status and Git operations can give
# stale and regenerated files identical timestamps. The staged snapshot is the
# commit candidate, so this check reasons about the Git index instead.

set -eu

if test "${1:-}" = "--simulate"; then
    staged_files=${2:-}
    unstaged_files=${3:-}
else
    repo_root=$(git rev-parse --show-toplevel)
    cd "$repo_root"
    staged_files=$(git diff --cached --name-only --diff-filter=ACMRD)
    unstaged_files=$(git diff --name-only --diff-filter=ACMRD)
fi

STAGED_FILES=$staged_files UNSTAGED_FILES=$unstaged_files awk '
BEGIN {
    staged_count = split(ENVIRON["STAGED_FILES"], staged_paths, "\n");
    unstaged_count = split(ENVIRON["UNSTAGED_FILES"], unstaged_paths, "\n");

    for (i = 1; i <= staged_count; ++i) {
        if (staged_paths[i] != "") {
            staged[staged_paths[i]] = 1;
        }
    }
    for (i = 1; i <= unstaged_count; ++i) {
        if (unstaged_paths[i] != "") {
            unstaged[unstaged_paths[i]] = 1;
        }
    }

    failed = 0;
    for (source in staged) {
        if (source !~ /(^|\/)Makefile[.]am$/) {
            continue;
        }

        generated = source;
        sub(/[.]am$/, ".in", generated);
        if (!(generated in staged)) {
            printf("%s: staged without generated %s\n", source, generated) \
                > "/dev/stderr";
            failed = 1;
        }
        if ((source in unstaged) || (generated in unstaged)) {
            printf("%s: source/generated pair has unstaged changes\n", source) \
                > "/dev/stderr";
            failed = 1;
        }
    }

    if (failed) {
        print "Regenerate and stage each Makefile.am/Makefile.in pair together." \
            > "/dev/stderr";
        exit 1;
    }
}
'
