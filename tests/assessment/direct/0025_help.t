#!/bin/sh
# Policy: docs/quality/color-evaluation.md
# Help advertises both direct option spellings.
set -eux
echo "1..1"
set -v

value=$(${SIXEL_RUNTIME-} "${LSQA_PATH}" -H) || {
    printf '\n%s\n' "not ok 1 - help failed"
    exit 0
}
case "${value}" in *'-D, --direct'*) true ;; *) false ;; esac || {
    printf '\n%s\n' "not ok 1 - direct help missing"
    exit 0
}
printf '\n%s\n' "ok 1 - help"
exit 0
