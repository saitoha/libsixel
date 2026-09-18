#!/bin/sh
# Verify -H prints target-side usage text to stdout.

set -eux


printf '1..1\n'
set -v

help_text=$(${SIXEL_RUNTIME-} "${LSQA_PATH}" -H) || {
    echo "not ok" 1 - "lsqa -H should exit with success"
    exit 0
}

case "${help_text}" in *Usage:\ lsqa**) true ;; *) false ;; esac || {
    echo "not ok" 1 - "lsqa -H did not print usage"
    exit 0
}

case "${help_text}" in *Options:**) true ;; *) false ;; esac || {
    echo "not ok" 1 - "lsqa -H did not print options"
    exit 0
}

case "${help_text}" in *\<reference\>\ \[target\]**) true ;; *) false ;; esac || {
    echo "not ok" 1 - "lsqa -H did not describe target argument"
    exit 0
}

case "${help_text}" in *--dequantize=METHOD**) true ;; *) false ;; esac || {
    echo "not ok" 1 - "lsqa -H did not describe dequantize option"
    exit 0
}

case "${help_text}" in *\<reference\>\ \[output\]**) false ;; *) true ;; esac || {
    echo "not ok" 1 - "lsqa -H still describes target as output"
    exit 0
}

echo "ok" 1 - "lsqa -H printed target usage text"
exit 0
