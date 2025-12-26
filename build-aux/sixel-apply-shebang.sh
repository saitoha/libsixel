#!/bin/sh
# Apply a shebang from a template file to one or more executable targets.
# Usage: sixel-apply-shebang.sh SHEBANG_FILE TARGET [...]
set -eu

if [ "$#" -lt 2 ]; then
    echo "usage: $0 SHEBANG_FILE TARGET [...]" >&2
    exit 1
fi

shexpr="$1"
shift

if [ ! -f "$shexpr" ]; then
    echo "shebang file not found: $shexpr" >&2
    exit 1
fi

apply_shebang() {
    target="$1"

    if [ ! -f "$target" ]; then
        echo "target not found: $target" >&2
        return 1
    fi

    prefix=$(head -c 2 "$target" 2>/dev/null || true)
    if [ "$prefix" = "#!" ]; then
        chmod +x "$target"
        return 0
    fi

    tmpfile="${target}.shexec.tmp"
    cat "$shexpr" > "$tmpfile"
    last_char=$(tail -c 1 "$tmpfile" 2>/dev/null || true)
    if [ "$last_char" != "" ] && [ "$last_char" != "\n" ]; then
        printf '\n' >> "$tmpfile"
    fi
    cat "$target" >> "$tmpfile"
    mv "$tmpfile" "$target"
    chmod +x "$target"
}

status=0
for bin in "$@"; do
    if ! apply_shebang "$bin"; then
        status=1
    fi
done

exit $status
