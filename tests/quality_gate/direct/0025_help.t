#!/bin/sh
# Policy: docs/quality/color-evaluation.md
# Help advertises both direct option spellings.
set -eux
echo "1..1"
set -v

value=$(${SIXEL_RUNTIME-} "${LSQA_PATH}" -H) || {
    echo "not ok 1 - help failed"
    exit 0
}
case "${value}" in *'-D, --direct'*) true ;; *) false ;; esac || {
    echo "not ok 1 - direct help missing"
    exit 0
}
echo "ok 1 - help"
exit 0
