#!/bin/sh
# Policy: docs/testing/guide.md
set -eux
# shellcheck source=tests/build/layout/fixture.sh
. "${0%/*}/fixture.sh"
cases 198
add owner/behavior/0199_static.sh
add owner/behavior/0200_installed.ps1
add owner/behavior/0201_source.sh
reject '201 cases exceed 200'
