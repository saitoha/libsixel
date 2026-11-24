#!/bin/sh
# Published rows survive release of the producer reference.
# Policy: docs/encoder/output/delta-encoding.md
set -eux
echo '1..1'
set -v
${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" "encoder/indexed/0018_indexed_published_lifetime" || {
    printf '\n%s\n' 'not ok 1 - indexed_published_lifetime'
    exit 0
}
printf '\n%s\n' 'ok 1 - indexed_published_lifetime'
exit 0
