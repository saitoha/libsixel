#!/bin/sh
# Policy: docs/testing/guide.md
# Keep the build/test entry points and their maintained helpers findutils-free.
set -eu
echo '1..1'
"${PYTHON_STATICCHECK:-python3}" "${0%/*}/../python/no_find.py" "$1" || {
    echo 'not ok 1 - maintained build and test code does not invoke find'
    exit 1
}
echo 'ok 1 - maintained build and test code does not invoke find'
