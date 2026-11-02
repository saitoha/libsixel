#!/bin/sh
# Policy: docs/testing/guide.md
set -eux
# shellcheck source=tests/build/layout/fixture.sh
. "${0%/*}/fixture.sh"
index=1
while test "$index" -le 25; do
    add "owner$index/behavior/0001_case.t"
    index=$((index + 1))
done
reject '25 categories exceed 24'
