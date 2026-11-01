#!/bin/sh
# Emit TAP for codespell static check.

set -eu
source_files_helper=$(CDPATH='' cd -- "${0%/*}" && pwd -P)/list-source-files.sh

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
trap 'rm -f "$tmpfile"; rm -rf "$tmpdir"' EXIT
trap 'exit 1' HUP INT TERM

cd "$src_root"

# Keep codespell focused on production sources to avoid long runtimes.
sh "$source_files_helper" src --exclude-path 'src/stb_image.h' --exclude-path 'src/stb_image_write.h' --name '*.[ch]' --name '*.md' --name '*.1' --name '*.in' --name '*.am' --name '*.build' --name '*.t' --name 'LICENSE' --name '*.py' --name '*.rb' --name '*.pl' --name '*.thumbnailer' --name '*.sh'| LC_ALL=C sort > "$tmpfile"

total=$(wc -l < "$tmpfile" | awk '{print $1}')
progress_is_tty=0
test -t 1 && progress_is_tty=1

emit_plan

if test "$total" -eq 0; then
    emit_note "codespell: no target files found"
    emit_pass
    exit 0
fi

queue_file="$tmpdir/queue.bin"
failed_list="$tmpdir/failed.list"
tab_char=$(printf '\t')
while IFS= read -r file_path; do
    test -n "$file_path" || continue
    printf '%s\0' "$file_path"
done < "$tmpfile" > "$queue_file"

: > "$failed_list"

done_count=0
ok_count=0
failed_count=0
scheduler_status=0

export CODESPELL_BIN="$codespell_bin"
export CODESPELL_LOG_DIR="$tmpdir"
# Stream results in the foreground so Ctrl+C reaches both pipeline stages.
collect_results() {
    scheduler_status=missing
    while IFS="$tab_char" read -r result id item_count; do
        test -n "$result" || continue
        if test "$result" = STATUS; then
            scheduler_status=$id
            continue
        fi
        done_count=$((done_count + item_count))
        case "$result" in
            OK)
                ok_count=$((ok_count + item_count))
                ;;
            NG)
                failed_count=$((failed_count + 1))
                printf '%s\n' "$id" >> "$failed_list"
                ;;
        esac
        percent=$((done_count * 100 / total))
        if test "$progress_is_tty" -eq 1; then
            printf '\r[codespell] %3d%% (%d/%d) clean_files=%d failed_batches=%d' \
                "$percent" "$done_count" "$total" "$ok_count" "$failed_count"
        fi
    done

    if test "$progress_is_tty" -eq 1; then
        printf '\n'
    fi

    case "$scheduler_status" in
        0|123)
            ;;
        *)
            emit_note "codespell: batch scheduler failed with status $scheduler_status"
            failed_count=$((failed_count + 1))
            ;;
    esac

    if test -s "$failed_list"; then
        while IFS= read -r id; do
            test -n "$id" || continue
            emit_note "codespell failure in batch $id:"
            cat "$tmpdir/$id.log"
        done < "$failed_list"
    fi

    test "$done_count" -eq "$total" || {
        emit_note "codespell: checked $done_count of $total files"
        failed_count=$((failed_count + 1))
    }
    if test "$failed_count" -eq 0; then
        emit_pass
        exit 0
    fi

    emit_fail
    exit 1
}
{
    # shellcheck disable=SC2016
    # Amortize Python startup and dictionary loading across bounded batches.
    sh "$src_root/tests/_static/sh/run-check-batches.sh" 64 "$queue_file" sh -c '
        item_id=$$
        log_path="${CODESPELL_LOG_DIR}/${item_id}.log"
        # "doub"/"alis" are PSD descriptor types; "Alph" is a PSB section key.
        if "$CODESPELL_BIN" -L "ser,sie,doub,alis,alph" "$@" >"$log_path" 2>&1; then
            printf "OK\t%s\t%s\n" "$item_id" "$#"
        else
            printf "NG\t%s\t%s\n" "$item_id" "$#"
        fi
    ' sh || scheduler_status=$?
    printf 'STATUS\t%s\t0\n' "$scheduler_status"

} | collect_results
