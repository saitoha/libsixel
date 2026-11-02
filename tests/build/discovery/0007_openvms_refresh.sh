#!/bin/sh
# Policy: docs/build/test-harness.md
set -eux
openvms=1
# shellcheck source=tests/build/discovery/fixture.sh
. "${0%/*}/fixture.sh"
add sample/existing/0001_old.t
expect sample/existing/0001_old.t
mv "$source_root/sample/existing/0001_old.t" "$source_root/sample/existing/0001_renamed.t"
expect sample/existing/0001_renamed.t
