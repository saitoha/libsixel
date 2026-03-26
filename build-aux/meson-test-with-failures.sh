#!/bin/sh
# Run `meson test` and print failed test names after Meson's summary.

set -eu

builddir=.
expect_builddir_arg=no
for arg in "$@"; do
    if test "$expect_builddir_arg" = yes; then
        builddir=$arg
        expect_builddir_arg=no
        continue
    fi
    case "$arg" in
        -C)
            expect_builddir_arg=yes
            ;;
        -C*)
            builddir=${arg#-C}
            ;;
    esac
done

if test "$expect_builddir_arg" = yes; then
    echo "meson-test-with-failures: -C requires a build directory argument" >&2
    exit 2
fi

set +e
meson test "$@"
meson_status=$?
set -e

testlog_json=$builddir/meson-logs/testlog.json
if test ! -r "$testlog_json"; then
    if test "$meson_status" -ne 0; then
        echo "================================================================"
        echo "Failed test list is unavailable (missing $testlog_json)."
    fi
    exit "$meson_status"
fi

failed_tests=$(awk '
    {
        name = ""
        result = ""

        if (index($0, "\"name\"") > 0) {
            name = $0
            sub(/^.*"name"[[:space:]]*:[[:space:]]*"/, "", name)
            sub(/".*$/, "", name)
        }
        if (index($0, "\"result\"") > 0) {
            result = $0
            sub(/^.*"result"[[:space:]]*:[[:space:]]*"/, "", result)
            sub(/".*$/, "", result)
        }

        if (name != "" &&
            (result == "FAIL" || result == "ERROR" || result == "TIMEOUT" ||
             result == "INTERRUPT" || result == "UNEXPECTEDPASS")) {
            printf "%s: %s\n", result, name
        }
    }
' "$testlog_json")

if test -n "$failed_tests"; then
    echo "================================================================"
    echo "Failed tests"
    echo "================================================================"
    printf '%s\n' "$failed_tests" | sed 's/^/# /'
fi

exit "$meson_status"
