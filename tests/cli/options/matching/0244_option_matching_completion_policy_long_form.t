#!/bin/sh
# Verify the converter-owned completion policy keeps long-key parsing.

set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    echo "1..0 # SKIP img2sixel is disabled in this build"
    exit 0
}

echo "1..1"
set -v

msg=$(set +xv; SIXEL_TRACE_TOPIC=suboption_contract \
    ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
    -K "a:bash_path=/missing-completion-source" -1 bash 2>&1) || {
    echo "not ok" 1 - "completion policy long form was rejected"
    exit 0
}
test "${msg#*LSXSUB1|*key=bash_path|stored=1|binding=bash_path,bash_path_override|value=*missing-completion-source*}" != "${msg}" || {
    echo "not ok" 1 - "completion policy long form was not stored"
    exit 0
}

echo "ok" 1 - "completion policy accepts base prefix and long key"
exit 0
