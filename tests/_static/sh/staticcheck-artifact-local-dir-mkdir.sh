#!/bin/sh
# Emit TAP for ARTIFACT_LOCAL_DIR directory bootstrap policy.
#
# Tests that reference ARTIFACT_LOCAL_DIR often write logs or output files
# under that directory. This static check requires each such test file to
# include at least one mkdir -p command that also references
# ARTIFACT_LOCAL_DIR, so missing-directory regressions are caught before
# runtime.

set -eu

src_root=$1
tests_root=$src_root/tests

echo "1..1"

if test ! -d "$tests_root"; then
    echo "not ok 1 - artifact local dir mkdir policy"
    echo "# tests directory not found: $tests_root"
    exit 1
fi

tmpfile=$(mktemp "${TMPDIR:-/tmp}/libsixel-artifact-local-dir-tests-XXXXXX")
cleanup() {
    rm -f "$tmpfile"
}
trap cleanup EXIT HUP INT TERM

(
    cd "$tests_root"
    find . \
        \( \
            -path './.perl-test-venv' -o \
            -path './.php-test-venv' -o \
            -path './.python-test-venv' -o \
            -path './.ruby-test-venv' -o \
            -path './_artifacts' -o \
            -path './data' \
        \) -prune -o \
        -type f -name '*.t' -print
) | LC_ALL=C sort > "$tmpfile"

if test ! -s "$tmpfile"; then
    echo "ok 1 # SKIP no TAP shell tests found"
    exit 0
fi

failed=0
while IFS= read -r relpath; do
    test -n "$relpath" || continue
    relpath=${relpath#./}
    file=$tests_root/$relpath
    if ! awk -v file="$relpath" '
BEGIN {
    uses_artifact_local_dir = 0
    has_artifact_local_dir_mkdir = 0
}
# Ignore full-line comments to reduce false positives from documentation.
$0 ~ /^[[:space:]]*#/ { next }
{
    if ($0 ~ /ARTIFACT_LOCAL_DIR/) {
        uses_artifact_local_dir = 1
    }

    if ($0 ~ /mkdir[[:space:]]+-p/ && $0 ~ /ARTIFACT_LOCAL_DIR/) {
        has_artifact_local_dir_mkdir = 1
    }
}
END {
    if (uses_artifact_local_dir && !has_artifact_local_dir_mkdir) {
        printf "# %s: ARTIFACT_LOCAL_DIR is referenced but mkdir -p with ARTIFACT_LOCAL_DIR is missing\n",
            file
        exit 1
    }
}
' "$file"; then
        failed=1
    fi
done < "$tmpfile"

if test "$failed" -ne 0; then
    echo "not ok 1 - ARTIFACT_LOCAL_DIR users create the artifact directory"
    exit 1
fi

echo "ok 1 - ARTIFACT_LOCAL_DIR users create the artifact directory"
