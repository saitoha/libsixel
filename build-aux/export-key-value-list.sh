#!/bin/sh
# Render shell-escaped KEY=VALUE assignments for selected environment keys.
# The output is intended for a trusted eval site such as:
#   eval "export $(.../export-key-value-list.sh <list>)"

set -eu

if [ "$#" -ne 1 ]; then
    echo "Usage: $0 <key-list-file>" >&2
    exit 1
fi

list_file=$1

if [ ! -f "$list_file" ]; then
    echo "Key list not found: $list_file" >&2
    exit 1
fi

quote_value() {
    printf "%s" "$1" | sed "s/'/'\\''/g"
}

while IFS= read -r key || [ -n "$key" ]; do
    case "$key" in
        ''|'#'*)
            continue
            ;;
    esac

    case "$key" in
        *[!A-Za-z0-9_]*|[0-9]* )
            echo "Invalid key name: $key" >&2
            exit 1
            ;;
    esac

    value=""
    eval "value=\${$key-}"
    escaped_value=$(quote_value "$value")
    printf "%s='%s' " "$key" "$escaped_value"
done < "$list_file"

printf "\n"
