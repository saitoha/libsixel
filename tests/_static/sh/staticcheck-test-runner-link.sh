#!/bin/sh
# Validate bounded link recipes and the object inventory without compiling.
# Policy: docs/build/test-harness.md
set -eux
echo '1..1'
set -v

"${2:-python3}" "$1/tests/_static/python/test_runner_link.py" "$1" || {
    echo 'not ok 1 - test runner object inventory and bounded link recipes'
    exit 0
}
echo 'ok 1 - test runner object inventory and bounded link recipes'
exit 0
