#!/bin/sh
# Convert plain newline-delimited metadata files into space-delimited strings.
# Supported modes:
#   tests   -> list of TAP test file paths
#   exports -> list of environment variable names to export

set -eu

if [ "$#" -ne 2 ]; then
    echo "Usage: $0 <tests|exports> <file>" >&2
    exit 1
fi

mode=$1
list_file=$2

if [ ! -f "$list_file" ]; then
    echo "Metadata list not found: $list_file" >&2
    exit 1
fi

case "$mode" in
    tests|exports)
        ;;
    *)
        echo "Unknown mode: $mode" >&2
        exit 1
        ;;
esac

awk '
    /^[[:space:]]*($|#)/ { next }
    {
        gsub(/^[[:space:]]+/, "")
        gsub(/[[:space:]]+$/, "")
        if ($0 != "") {
            printf "%s ", $0
        }
    }
    END {
        printf "\n"
    }
' "$list_file"
