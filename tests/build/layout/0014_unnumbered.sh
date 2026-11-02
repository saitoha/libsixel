#!/bin/sh
# Policy: docs/testing/guide.md
set -eux
# shellcheck source=tests/build/layout/fixture.sh
. "${0%/*}/fixture.sh"
add owner/behavior/unnumbered.t
reject 'expected NNNN_observation.ext'
