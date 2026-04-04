#!/bin/sh
# Emit TAP for codespell static check.

set -eu

src_root=$1
codespell_bin=$2
output_mode=${SIXEL_STATICCHECK_MODE:-tap}

emit_plan() {
    if test "$output_mode" = "tap"; then
        echo "1..1"
    fi
}

emit_skip() {
    reason=$1
    if test "$output_mode" = "tap"; then
        echo "1..0 # SKIP $reason"
    else
        echo "SKIP: $reason"
    fi
    exit 0
}

emit_note() {
    message=$1
    if test "$output_mode" = "tap"; then
        printf '# %s\n' "$message"
    else
        printf '%s\n' "$message"
    fi
}

emit_pass() {
    if test "$output_mode" = "tap"; then
        echo "ok 1 - codespell"
    else
        echo "PASS: codespell"
    fi
}

emit_fail() {
    if test "$output_mode" = "tap"; then
        echo "not ok 1 - codespell"
    else
        echo "FAIL: codespell"
    fi
}

if test -z "$codespell_bin"; then
    emit_skip "codespell not found"
fi

if test ! -x "$codespell_bin" && ! command -v "$codespell_bin" >/dev/null 2>&1; then
    emit_skip "codespell executable not found: $codespell_bin"
fi

tmpfile=$(mktemp "${TMPDIR:-/tmp}/libsixel-codespell-files-XXXXXX")
tmpdir=$(mktemp -d "${TMPDIR:-/tmp}/libsixel-codespell-run-XXXXXX")
trap 'rm -f "$tmpfile"; rm -rf "$tmpdir"' EXIT HUP INT TERM

cd "$src_root"

# Keep codespell focused on production sources to avoid long runtimes.
find src \
    \( -path 'src/stb_image.h' -o -path 'src/stb_image_write.h' \) -prune -o \
    -type f \( -name '*.[ch]' -o -name '*.md' -o \
        -name '*.1' -o -name '*.in' -o -name '*.am' -o \
        -name '*.build' -o -name '*.t' -o -name 'LICENSE' -o \
        -name '*.py' -o -name '*.rb' -o -name '*.pl' -o \
        -name '*.thumbnailer' -o -name '*.sh' \) -print \
    | LC_ALL=C sort > "$tmpfile"

total=$(wc -l < "$tmpfile" | awk '{print $1}')
jobs=${STATICCHECK_JOBS:-}
if test -z "$jobs"; then
    jobs=$(getconf _NPROCESSORS_ONLN 2>/dev/null || printf '4')
fi
case "$jobs" in
    ''|*[!0-9]*|0)
        jobs=1
        ;;
esac
progress_is_tty=0
test -t 1 && progress_is_tty=1

emit_plan

if test "$total" -eq 0; then
    emit_note "codespell: no target files found"
    emit_pass
    exit 0
fi

queue_file="$tmpdir/queue.bin"
result_fifo="$tmpdir/result.fifo"
failed_list="$tmpdir/failed.list"
tab_char=$(printf '\t')
index=0
while IFS= read -r file_path; do
    test -n "$file_path" || continue
    index=$((index + 1))
    printf '%s\0%s\0' "$index" "$file_path" >> "$queue_file"
done < "$tmpfile"

mkfifo "$result_fifo"
: > "$failed_list"

done_count=0
ok_count=0
failed_count=0
xargs_status=0

export CODESPELL_BIN="$codespell_bin"
export CODESPELL_LOG_DIR="$tmpdir"
# shellcheck disable=SC2016
xargs -0 -n 2 -P "$jobs" sh -c '
    item_id=$1
    item_path=$2
    log_path="${CODESPELL_LOG_DIR}/${item_id}.log"
    # "doub" and "alis" are PSD descriptor type identifiers.
    if "$CODESPELL_BIN" -L "ser,sie,doub,alis" "$item_path" >"$log_path" 2>&1; then
        printf "OK\t%s\t%s\n" "$item_id" "$item_path"
    else
        printf "NG\t%s\t%s\n" "$item_id" "$item_path"
    fi
' sh < "$queue_file" > "$result_fifo" &
worker_pid=$!
unset CODESPELL_BIN CODESPELL_LOG_DIR

while IFS="$tab_char" read -r result id file_path; do
    test -n "$result" || continue
    done_count=$((done_count + 1))
    case "$result" in
        OK)
            ok_count=$((ok_count + 1))
            ;;
        NG)
            failed_count=$((failed_count + 1))
            printf '%s\t%s\n' "$id" "$file_path" >> "$failed_list"
            ;;
    esac
    percent=$((done_count * 100 / total))
    if test "$progress_is_tty" -eq 1; then
        printf '\r[codespell] %3d%% (%d/%d) success=%d failed=%d' \
            "$percent" "$done_count" "$total" "$ok_count" "$failed_count"
    fi
done < "$result_fifo"

wait "$worker_pid" || xargs_status=$?
if test "$progress_is_tty" -eq 1; then
    printf '\n'
fi

case "$xargs_status" in
    0|123)
        ;;
    *)
        emit_note "codespell: xargs failed with status $xargs_status"
        failed_count=$((failed_count + 1))
        ;;
esac

if test -s "$failed_list"; then
    while IFS="$tab_char" read -r id file_path; do
        test -n "$id" || continue
        emit_note "codespell failure: $file_path"
        sed -n '1,120p' "$tmpdir/$id.log"
    done < "$failed_list"
fi

if test "$failed_count" -eq 0; then
    emit_pass
    exit 0
fi

emit_fail
exit 1
