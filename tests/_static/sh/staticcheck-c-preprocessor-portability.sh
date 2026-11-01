#!/bin/sh
# Check macro collisions and fallback ordering without a target compiler.
# Policy: docs/testing/staticcheck.md
set -eux
echo '1..1'
set -v
"${2:-python3}" "$1/tests/_static/python/c_preprocessor_portability.py" "$1" || {
    echo 'not ok 1 - C identifiers and integer fallback ordering are portable'
    exit 0
}
echo 'ok 1 - C identifiers and integer fallback ordering are portable'
exit 0
