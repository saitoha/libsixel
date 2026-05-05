#!/bin/sh
# Run static checks in deterministic order and present concise plain logs.

set -eu

src_root=${1:-}
build_root=${2:-}
actionlint_bin=${3:-}
shellcheck_driver=${4:-}
shellcheck_bin=${5:-}
codespell_bin=${6:-}
python_bin=${7:-}
have_tree_sitter_c=${8:-auto}

test -n "$src_root" || {
    echo "Usage: $0 <src_root> <build_root> <actionlint> <shellcheck_driver> <shellcheck> <codespell> <python> [have_tree_sitter_c]" >&2
    exit 2
}

test -n "$build_root" || build_root="$src_root"

tmpdir=$(mktemp -d "${TMPDIR:-/tmp}/libsixel-staticcheck-suite-XXXXXX")
trap 'rm -rf "$tmpdir"' EXIT HUP INT TERM

TOP_SRCDIR=${TOP_SRCDIR:-$src_root}
TOP_BUILDDIR=${TOP_BUILDDIR:-$build_root}
ARTIFACT_ROOT=${ARTIFACT_ROOT:-$build_root/tests/_artifacts}
export TOP_SRCDIR TOP_BUILDDIR ARTIFACT_ROOT

total=$(awk '
$1 ~ /^run_case_(tap|plain|skip)$/ {
    name=$2
    gsub(/"/, "", name)
    if (name ~ /^staticcheck-/) {
        seen[name]=1
    }
}
END {
    count=0
    for (name in seen) {
        count++
    }
    print count + 0
}
' "$0")
index=0
pass_count=0
skip_count=0
fail_count=0

strip_tap_log() {
    log_path=$1
    awk '
    /^1\.\.0[[:space:]]*#[[:space:]]*[Ss][Kk][Ii][Pp][[:space:]]*/ {
        line=$0
        sub(/^1\.\.0[[:space:]]*#[[:space:]]*[Ss][Kk][Ii][Pp][[:space:]]*/, "SKIP: ", line)
        print line
        next
    }
    /^ok[[:space:]]+[0-9]+[[:space:]]*#[[:space:]]*[Ss][Kk][Ii][Pp][[:space:]]*/ {
        line=$0
        sub(/^ok[[:space:]]+[0-9]+[[:space:]]*#[[:space:]]*[Ss][Kk][Ii][Pp][[:space:]]*/, "SKIP: ", line)
        print line
        next
    }
    /^1\.\.[0-9]+([[:space:]]|$)/ { next }
    /^(not[[:space:]]+)?ok([[:space:]]|$)/ { next }
    {
        line=$0
        sub(/^#[[:space:]]?/, "", line)
        if (line != "") {
            print line
        }
    }
    ' "$log_path"
}

is_tap_skip() {
    log_path=$1
    awk '
    /^1\.\.0[[:space:]]*#[[:space:]]*[Ss][Kk][Ii][Pp]([[:space:]]|$)/ { found=1 }
    /^ok[[:space:]]+[0-9]+[[:space:]]*#[[:space:]]*[Ss][Kk][Ii][Pp]([[:space:]]|$)/ { found=1 }
    END { exit found ? 0 : 1 }
    ' "$log_path"
}

tool_is_available() {
    tool_path=$1
    test -n "$tool_path" || return 1
    test -x "$tool_path" && return 0
    command -v "$tool_path" >/dev/null 2>&1
}

run_case_tap() {
    case_name=$1
    shift

    index=$((index + 1))
    log_path="$tmpdir/${index}.log"
    filtered_log="$tmpdir/${index}.filtered"

    printf '==> [%02d/%02d] %s\n' "$index" "$total" "$case_name"

    if "$@" >"$log_path" 2>&1; then
        case_rc=0
    else
        case_rc=$?
    fi

    strip_tap_log "$log_path" > "$filtered_log"

    if test "$case_rc" -eq 0; then
        if is_tap_skip "$log_path"; then
            skip_count=$((skip_count + 1))
            printf 'SKIP %s\n' "$case_name"
            if test -s "$filtered_log"; then
                cat "$filtered_log"
            fi
        else
            pass_count=$((pass_count + 1))
            printf 'PASS %s\n' "$case_name"
        fi
        return 0
    fi

    fail_count=$((fail_count + 1))
    printf 'FAIL %s\n' "$case_name"
    if test -s "$filtered_log"; then
        cat "$filtered_log"
    else
        sed -n '1,120p' "$log_path"
    fi
    return "$case_rc"
}

run_case_plain() {
    case_name=$1
    shift

    index=$((index + 1))
    log_path="$tmpdir/${index}.plain.log"
    printf '==> [%02d/%02d] %s\n' "$index" "$total" "$case_name"

    if "$@" >"$log_path" 2>&1; then
        if test -s "$log_path"; then
            cat "$log_path"
        fi
        pass_count=$((pass_count + 1))
        printf 'PASS %s\n' "$case_name"
        return 0
    fi

    if test -s "$log_path"; then
        cat "$log_path"
    fi
    fail_count=$((fail_count + 1))
    printf 'FAIL %s\n' "$case_name"
    return 1
}

run_case_skip() {
    case_name=$1
    reason=$2

    index=$((index + 1))
    skip_count=$((skip_count + 1))
    printf '==> [%02d/%02d] %s\n' "$index" "$total" "$case_name"
    printf 'SKIP %s\n' "$case_name"
    printf 'SKIP: %s\n' "$reason"
}

fail_and_exit() {
    rc=$1
    printf 'staticcheck summary: total=%d pass=%d skip=%d fail=%d\n' \
        "$index" "$pass_count" "$skip_count" "$fail_count"
    exit "$rc"
}

# shellcheck disable=SC2329
run_staticcheck_webp_strict_compile() {
    source_rel=$1
    cc_bin=${2:-${CC:-cc}}
    source_path=$src_root/src/$source_rel
    object_path=$tmpdir/${source_rel##*/}.${cc_bin##*/}.strict.o

    "$cc_bin" -DHAVE_CONFIG_H \
        -I"$build_root" -I"$build_root/include" \
        -I"$src_root" -I"$src_root/src" -I"$src_root/include" \
        -std=c99 -Wall -Wextra -Wpedantic \
        -Wconversion -Wsign-conversion -Wtype-limits -Werror \
        -c "$source_path" -o "$object_path"
}

# shellcheck disable=SC2329
run_staticcheck_threading_no_threads_compile() {
    cc_bin=${1:-${CC:-cc}}
    config_dir=$tmpdir/threading-no-threads-config
    object_path=$tmpdir/threading.${cc_bin##*/}.no-threads.o

    mkdir -p "$config_dir"
    cp "$build_root/config.h" "$config_dir/config.h"
    cat >> "$config_dir/config.h" <<'EOF'
#undef SIXEL_ENABLE_THREADS
#define SIXEL_ENABLE_THREADS 0
EOF

    "$cc_bin" -DHAVE_CONFIG_H \
        -I"$config_dir" -I"$build_root/include" \
        -I"$src_root" -I"$src_root/src" -I"$src_root/include" \
        -std=c99 -Wall -Wextra -Wpedantic -Werror \
        -c "$src_root/src/threading.c" -o "$object_path"
}

# shellcheck disable=SC2329
run_staticcheck_timeline_logger_no_threads_symbols() {
    cc_bin=${1:-${CC:-cc}}
    config_dir=$tmpdir/timeline-logger-no-threads-config
    object_path=$tmpdir/timeline-logger.${cc_bin##*/}.no-threads.o
    undefined_path=$tmpdir/timeline-logger.no-threads.undefined

    mkdir -p "$config_dir"
    cp "$build_root/config.h" "$config_dir/config.h"
    cat >> "$config_dir/config.h" <<'EOF'
#undef SIXEL_ENABLE_THREADS
#define SIXEL_ENABLE_THREADS 0
EOF

    "$cc_bin" -DHAVE_CONFIG_H \
        -I"$config_dir" -I"$build_root/include" \
        -I"$src_root" -I"$src_root/src" -I"$src_root/include" \
        -std=c99 -Wall -Wextra -Wpedantic -Werror \
        -c "$src_root/src/timeline-logger.c" -o "$object_path"

    nm -u "$object_path" > "$undefined_path"
    awk '
    /sixel_mutex_|sixel_cond_|sixel_thread_/ {
        print
        found = 1
    }
    END {
        exit found ? 1 : 0
    }
    ' "$undefined_path"
}

# shellcheck disable=SC2329
run_staticcheck_threadpool_no_threads_symbols() {
    cc_bin=${1:-${CC:-cc}}
    config_dir=$tmpdir/threadpool-no-threads-config
    object_path=$tmpdir/threadpool.${cc_bin##*/}.no-threads.o
    undefined_path=$tmpdir/threadpool.no-threads.undefined

    mkdir -p "$config_dir"
    cp "$build_root/config.h" "$config_dir/config.h"
    cat >> "$config_dir/config.h" <<'EOF'
#undef SIXEL_ENABLE_THREADS
#define SIXEL_ENABLE_THREADS 0
EOF

    "$cc_bin" -DHAVE_CONFIG_H \
        -I"$config_dir" -I"$build_root/include" \
        -I"$src_root" -I"$src_root/src" -I"$src_root/include" \
        -std=c99 -Wall -Wextra -Wpedantic -Werror \
        -c "$src_root/src/threadpool.c" -o "$object_path"

    nm -u "$object_path" > "$undefined_path"
    awk '
    /sixel_mutex_|sixel_cond_|sixel_thread_/ {
        print
        found = 1
    }
    END {
        exit found ? 1 : 0
    }
    ' "$undefined_path"
}

# shellcheck disable=SC2329
run_staticcheck_threadpool_vtbl_boundary() {
    violations=$tmpdir/threadpool-vtbl-boundary.txt

    find "$src_root/src" -type f \( -name '*.c' -o -name '*.h' \) \
        ! -name 'threadpool.c' ! -name 'threadpool.h' \
        -exec awk '
        /(^|[^A-Za-z0-9_])threadpool_(create|set_affinity|destroy|push|finish|get_error|grow)[ \t]*\(/ {
            print FILENAME ":" FNR ": direct threadpool free-function call"
        }
        ' {} + > "$violations"
    test ! -s "$violations" || {
        cat "$violations"
        return 1
    }
}

# shellcheck disable=SC2329
run_staticcheck_webp_tables_self_include() {
    cc_bin=${1:-${CC:-cc}}
    source_path=$tmpdir/staticcheck-webp-vp8-tables-self-include.c
    object_path=$tmpdir/staticcheck-webp-vp8-tables-self-include.${cc_bin##*/}.o

    cat > "$source_path" <<'EOF'
#if defined(HAVE_CONFIG_H)
# include "config.h"
#endif

#include "fromwebp-vp8-tables.h"

int
sixel_staticcheck_webp_vp8_tables_probe(void)
{
    return (int)sixel_webp_vp8_default_coef_probs[0][0][0][0];
}
EOF

    "$cc_bin" -DHAVE_CONFIG_H \
        -I"$build_root" -I"$build_root/include" \
        -I"$src_root" -I"$src_root/src" -I"$src_root/include" \
        -std=c99 -Wall -Wextra -Wpedantic -Werror \
        -c "$source_path" -o "$object_path"
}

run_case_tap "staticcheck-private-includes" \
    "$src_root/tests/_static/sh/staticcheck-private-includes.sh" \
    "$src_root" "$python_bin" || fail_and_exit $?

run_case_tap "staticcheck-src-no-direct-getenv" \
    "$src_root/tests/_static/sh/staticcheck-src-no-direct-getenv.sh" \
    "$src_root" || fail_and_exit $?

run_case_tap "staticcheck-test-no-direct-getenv" \
    "$src_root/tests/_static/sh/staticcheck-test-no-direct-getenv.sh" \
    "$src_root" || fail_and_exit $?

run_case_tap "staticcheck-c-ast" \
    "$src_root/tests/_static/sh/staticcheck-c-ast.sh" \
    "$src_root" "$python_bin" "$have_tree_sitter_c" || fail_and_exit $?

run_case_tap "staticcheck-positional-bluenoise-threadsafe-init" \
    sh "$src_root/tests/_static/sh/staticcheck-positional-bluenoise-threadsafe-init.sh" \
    "$src_root" || fail_and_exit $?

run_case_tap "staticcheck-c-size-max-header" \
    "$src_root/tests/_static/sh/staticcheck-c-size-max-header.sh" \
    "$src_root" || fail_and_exit $?

run_case_tap "staticcheck-c-identifier-significant-length" \
    "$src_root/tests/_static/sh/staticcheck-c-identifier-significant-length.sh" \
    "$src_root" || fail_and_exit $?

run_case_tap "staticcheck-makefile-recipes" \
    "$src_root/tests/_static/sh/staticcheck-makefile-recipes.sh" \
    "$src_root" || fail_and_exit $?

run_case_tap "staticcheck-psb-large-fixture-clean-sync" \
    "$src_root/tests/_static/sh/staticcheck-psb-large-fixture-clean-sync.sh" \
    "$src_root" || fail_and_exit $?

run_case_tap "staticcheck-src-makefile-dist-sources-sync" \
    "$src_root/tests/_static/sh/staticcheck-src-makefile-dist-sources-sync.sh" \
    "$src_root" || fail_and_exit $?

run_case_tap "staticcheck-test-inc-extra-dist-sync" \
    "$src_root/tests/_static/sh/staticcheck-test-inc-extra-dist-sync.sh" \
    "$src_root" || fail_and_exit $?

run_case_tap "staticcheck-meson-palette-sources" \
    "$src_root/tests/_static/sh/staticcheck-meson-palette-sources.sh" \
    "$src_root" || fail_and_exit $?

run_case_tap "staticcheck-amalgamation-static-symbols" \
    "$src_root/tests/_static/sh/staticcheck-amalgamation-static-symbols.sh" \
    "$src_root" || fail_and_exit $?

run_case_tap "staticcheck-kmedoids-suboption-env-sync" \
    "$src_root/tests/_static/sh/staticcheck-kmedoids-suboption-env-sync.sh" \
    "$src_root" || fail_and_exit $?

run_case_tap "staticcheck-kcenter-suboption-env-sync" \
    "$src_root/tests/_static/sh/staticcheck-kcenter-suboption-env-sync.sh" \
    "$src_root" || fail_and_exit $?

run_case_tap "staticcheck-completion-medoids-tokens" \
    "$src_root/tests/_static/sh/staticcheck-completion-medoids-tokens.sh" \
    "$src_root" || fail_and_exit $?

run_case_tap "staticcheck-kmeans-suboption-env-sync" \
    "$src_root/tests/_static/sh/staticcheck-kmeans-suboption-env-sync.sh" \
    "$src_root" || fail_and_exit $?

run_case_tap "staticcheck-lookup-suboption-shared-sync" \
    "$src_root/tests/_static/sh/staticcheck-lookup-suboption-shared-sync.sh" \
    "$src_root" || fail_and_exit $?

run_case_tap "staticcheck-interframe-strategy-token-sync" \
    sh "$src_root/tests/_static/sh/staticcheck-interframe-strategy-token-sync.sh" \
    "$src_root" || fail_and_exit $?

run_case_tap "staticcheck-interframe-c-h-pair-sync" \
    "$src_root/tests/_static/sh/staticcheck-interframe-c-h-pair-sync.sh" \
    "$src_root" || fail_and_exit $?

run_case_tap "staticcheck-interframe-no-compat-getenv-symbol" \
    "$src_root/tests/_static/sh/staticcheck-interframe-no-compat-getenv-symbol.sh" \
    "$src_root" || fail_and_exit $?

run_case_tap "staticcheck-src-config-first-include" \
    "$src_root/tests/_static/sh/staticcheck-src-config-first-include.sh" \
    "$src_root" || fail_and_exit $?

run_case_tap "staticcheck-diffusion-legacy-scan-option" \
    "$src_root/tests/_static/sh/staticcheck-diffusion-legacy-scan-option.sh" \
    "$src_root" || fail_and_exit $?

run_case_tap "staticcheck-gd-policy-wrapper-sync" \
    "$src_root/tests/_static/sh/staticcheck-gd-policy-wrapper-sync.sh" \
    "$src_root" || fail_and_exit $?

run_case_tap "staticcheck-gd-optional-fallback-profile-sync" \
    "$src_root/tests/_static/sh/staticcheck-gd-optional-fallback-profile-sync.sh" \
    "$src_root" || fail_and_exit $?

run_case_tap "staticcheck-quantize-merge-suboption-sync" \
    "$src_root/tests/_static/sh/staticcheck-quantize-merge-suboption-sync.sh" \
    "$src_root" || fail_and_exit $?

run_case_tap "staticcheck-test-runner-dllexport" \
    "$src_root/tests/_static/sh/staticcheck-test-runner-dllexport.sh" \
    "$src_root" || fail_and_exit $?

run_case_tap "staticcheck-lookup-backend-exports" \
    "$src_root/tests/_static/sh/staticcheck-lookup-backend-exports.sh" \
    "$src_root" || fail_and_exit $?

run_case_tap "staticcheck-public-compat" \
    "$src_root/tests/_static/sh/staticcheck-public-compat.sh" \
    "$src_root" "$build_root" || fail_and_exit $?

run_case_tap "staticcheck-6cells-idl-sync" \
    "$src_root/tests/_static/sh/staticcheck-6cells-idl-sync.sh" \
    "$src_root" || fail_and_exit $?

run_case_tap "staticcheck-6cells-contract-attrs" \
    "$src_root/tests/_static/sh/staticcheck-6cells-contract-attrs.sh" \
    "$src_root" || fail_and_exit $?

run_case_tap "staticcheck-frame-post187-export-guard" \
    "$src_root/tests/_static/sh/staticcheck-frame-post187-export-guard.sh" \
    "$src_root" || fail_and_exit $?

run_case_tap "staticcheck-frame-instantiation-boundary" \
    "$src_root/tests/_static/sh/staticcheck-frame-instantiation-boundary.sh" \
    "$src_root" || fail_and_exit $?

run_case_tap "staticcheck-output-instantiation-boundary" \
    "$src_root/tests/_static/sh/staticcheck-output-instantiation-boundary.sh" \
    "$src_root" || fail_and_exit $?

run_case_tap "staticcheck-chunk-component-boundary" \
    "$src_root/tests/_static/sh/staticcheck-chunk-component-boundary.sh" \
    "$src_root" || fail_and_exit $?

run_case_tap "staticcheck-chunk-factory-vtbl-guard" \
    "$src_root/tests/_static/sh/staticcheck-chunk-factory-vtbl-guard.sh" \
    "$src_root" || fail_and_exit $?

run_case_tap "staticcheck-chunk-source-path-format-guard" \
    "$src_root/tests/_static/sh/staticcheck-chunk-source-path-format-guard.sh" \
    "$src_root" || fail_and_exit $?

run_case_tap "staticcheck-fromwebp-chunk-view-bind" \
    "$src_root/tests/_static/sh/staticcheck-fromwebp-chunk-view-bind.sh" \
    "$src_root" || fail_and_exit $?

run_case_tap "staticcheck-frombmp-chunk-view-bind" \
    "$src_root/tests/_static/sh/staticcheck-frombmp-chunk-view-bind.sh" \
    "$src_root" || fail_and_exit $?

run_case_tap "staticcheck-deprecated-diagnostic-guard" \
    "$src_root/tests/_static/sh/staticcheck-deprecated-diagnostic-guard.sh" \
    "$src_root" || fail_and_exit $?

run_case_tap "staticcheck-lookup-policy-no-backend-includes" \
    "$src_root/tests/_static/sh/staticcheck-lookup-policy-no-backend-includes.sh" \
    "$src_root" || fail_and_exit $?

run_case_tap "staticcheck-factory-classid-registry-sync" \
    "$src_root/tests/_static/sh/staticcheck-factory-classid-registry-sync.sh" \
    "$src_root" || fail_and_exit $?

run_case_tap "staticcheck-serviceid-registry-sync" \
    "$src_root/tests/_static/sh/staticcheck-serviceid-registry-sync.sh" \
    "$src_root" || fail_and_exit $?

run_case_plain "staticcheck-threadpool-vtbl-boundary" \
    run_staticcheck_threadpool_vtbl_boundary || fail_and_exit $?

run_case_tap "staticcheck-timeline-logging-boundary" \
    "$src_root/tests/_static/sh/staticcheck-timeline-logging-boundary.sh" \
    "$src_root" || fail_and_exit $?

run_case_tap "staticcheck-dither-policy-no-backend-dispatch" \
    "$src_root/tests/_static/sh/staticcheck-dither-policy-no-backend-dispatch.sh" \
    "$src_root" || fail_and_exit $?

run_case_tap "staticcheck-dither-policy-create-analyzer-guard-sync" \
    "$src_root/tests/_static/sh/staticcheck-dither-policy-create-analyzer-guard-sync.sh" \
    "$src_root" || fail_and_exit $?

run_case_tap "staticcheck-dither-positional-macro-guarded-locals" \
    "$src_root/tests/_static/sh/staticcheck-dither-positional-macro-guarded-locals.sh" \
    "$src_root" || fail_and_exit $?

run_case_tap "staticcheck-dither-policy-warning-compile" \
    "$src_root/tests/_static/sh/staticcheck-dither-policy-warning-compile.sh" \
    "$src_root" "$build_root" || fail_and_exit $?

run_case_tap "staticcheck-test-runner-amalgamation-defines-sync" \
    "$src_root/tests/_static/sh/staticcheck-test-runner-amalgamation-defines-sync.sh" \
    "$src_root" || fail_and_exit $?

run_case_tap "staticcheck-gperf-generated-undefs" \
    "$src_root/tests/_static/sh/staticcheck-gperf-generated-undefs.sh" \
    "$src_root" || fail_and_exit $?

run_case_tap "staticcheck-binding-loader-setopt-sync" \
    "$src_root/tests/_static/sh/staticcheck-binding-loader-setopt-sync.sh" \
    "$src_root" || fail_and_exit $?

run_case_tap "staticcheck-python-compileall" \
    "$src_root/tests/_static/sh/staticcheck-python-compileall.sh" \
    "$src_root" "$python_bin" || fail_and_exit $?

run_case_tap "staticcheck-actionlint" \
    "$src_root/tests/_static/sh/staticcheck-actionlint.sh" \
    "$src_root" "$actionlint_bin" || fail_and_exit $?

if tool_is_available "$shellcheck_bin"; then
    run_case_plain "staticcheck-shellcheck" \
        env SIXEL_STATICCHECK_MODE=plain \
        "$src_root/tests/_static/sh/staticcheck-shellcheck.sh" \
        "$shellcheck_driver" "$src_root" "$shellcheck_bin" || fail_and_exit $?
else
    run_case_skip "staticcheck-shellcheck" "shellcheck not found"
fi

if tool_is_available "$codespell_bin"; then
    run_case_plain "staticcheck-codespell" \
        env SIXEL_STATICCHECK_MODE=plain \
        "$src_root/tests/_static/sh/staticcheck-codespell.sh" \
        "$src_root" "$codespell_bin" || fail_and_exit $?
else
    run_case_skip "staticcheck-codespell" "codespell not found"
fi

run_case_tap "staticcheck-test-plan-single" \
    "$src_root/tests/_static/sh/staticcheck-test-plan-single.sh" \
    "$src_root" || fail_and_exit $?

run_case_tap "staticcheck-test-webp-serial-unique" \
    "$src_root/tests/_static/sh/staticcheck-test-webp-serial-unique.sh" \
    "$src_root" || fail_and_exit $?

run_case_tap "staticcheck-artifact-local-dir-mkdir" \
    "$src_root/tests/_static/sh/staticcheck-artifact-local-dir-mkdir.sh" \
    "$src_root" || fail_and_exit $?

run_case_tap "staticcheck-test-no-grep-awk" \
    "$src_root/tests/_static/sh/staticcheck-test-no-grep-awk.sh" \
    "$src_root" || fail_and_exit $?

run_case_tap "staticcheck-c-header-modeline" \
    "$src_root/tests/_static/sh/staticcheck-c-header-modeline.sh" \
    "$src_root" || fail_and_exit $?

if test -f "$build_root/src/Makefile"; then
    run_case_plain "staticcheck-fromwebp-compile-type-limits" \
        "${MAKE:-make}" -C "$build_root/src" -W fromwebp.c \
        libsixel_la-fromwebp.lo || fail_and_exit $?
    run_case_plain "staticcheck-fromwebp-container-compile-warnings" \
        "${MAKE:-make}" -C "$build_root/src" -W fromwebp-container.c \
        libsixel_la-fromwebp-container.lo || fail_and_exit $?
    run_case_plain "staticcheck-fromwebp-vp8-compile-warnings" \
        "${MAKE:-make}" -C "$build_root/src" -W fromwebp-vp8.c \
        libsixel_la-fromwebp-vp8.lo || fail_and_exit $?
    run_case_plain "staticcheck-fromwebp-vp8-alpha-compile-warnings" \
        "${MAKE:-make}" -C "$build_root/src" -W fromwebp-vp8-alpha.c \
        libsixel_la-fromwebp-vp8-alpha.lo || fail_and_exit $?
    run_case_plain "staticcheck-fromwebp-vp8-parse-compile-warnings" \
        "${MAKE:-make}" -C "$build_root/src" -W fromwebp-vp8-parse.c \
        libsixel_la-fromwebp-vp8-parse.lo || fail_and_exit $?
    run_case_plain "staticcheck-fromwebp-vp8-native-compile-warnings" \
        "${MAKE:-make}" -C "$build_root/src" -W fromwebp-vp8-native.c \
        libsixel_la-fromwebp-vp8-native.lo || fail_and_exit $?
    run_case_plain "staticcheck-fromwebp-vp8-native-token-compile-warnings" \
        "${MAKE:-make}" -C "$build_root/src" -W fromwebp-vp8-native-token.c \
        libsixel_la-fromwebp-vp8-native-token.lo || fail_and_exit $?
    run_case_plain "staticcheck-fromwebp-vp8l-compile-warnings" \
        "${MAKE:-make}" -C "$build_root/src" -W fromwebp-vp8l.c \
        libsixel_la-fromwebp-vp8l.lo || fail_and_exit $?
    run_case_plain "staticcheck-fromwebp-trace-compile-warnings" \
        "${MAKE:-make}" -C "$build_root/src" -W fromwebp-trace.c \
        libsixel_la-fromwebp-trace.lo || fail_and_exit $?
else
    run_case_skip "staticcheck-fromwebp-compile-type-limits" \
        "missing src/Makefile in build root"
    run_case_skip "staticcheck-fromwebp-container-compile-warnings" \
        "missing src/Makefile in build root"
    run_case_skip "staticcheck-fromwebp-vp8-compile-warnings" \
        "missing src/Makefile in build root"
    run_case_skip "staticcheck-fromwebp-vp8-alpha-compile-warnings" \
        "missing src/Makefile in build root"
    run_case_skip "staticcheck-fromwebp-vp8-parse-compile-warnings" \
        "missing src/Makefile in build root"
    run_case_skip "staticcheck-fromwebp-vp8-native-compile-warnings" \
        "missing src/Makefile in build root"
    run_case_skip "staticcheck-fromwebp-vp8-native-token-compile-warnings" \
        "missing src/Makefile in build root"
    run_case_skip "staticcheck-fromwebp-vp8l-compile-warnings" \
        "missing src/Makefile in build root"
    run_case_skip "staticcheck-fromwebp-trace-compile-warnings" \
        "missing src/Makefile in build root"
fi

if test -f "$build_root/config.h"; then
    run_case_plain "staticcheck-threading-no-threads-compile" \
        run_staticcheck_threading_no_threads_compile || fail_and_exit $?
    run_case_plain "staticcheck-timeline-logger-no-threads-symbols" \
        run_staticcheck_timeline_logger_no_threads_symbols || fail_and_exit $?
    run_case_plain "staticcheck-threadpool-no-threads-symbols" \
        run_staticcheck_threadpool_no_threads_symbols || fail_and_exit $?
    run_case_plain "staticcheck-fromwebp-vp8-tables-self-include" \
        run_staticcheck_webp_tables_self_include || fail_and_exit $?
    run_case_plain "staticcheck-fromwebp-strict-compile" \
        run_staticcheck_webp_strict_compile fromwebp.c || fail_and_exit $?
    run_case_plain "staticcheck-fromwebp-container-strict-compile" \
        run_staticcheck_webp_strict_compile fromwebp-container.c || \
        fail_and_exit $?
    run_case_plain "staticcheck-fromwebp-vp8-strict-compile" \
        run_staticcheck_webp_strict_compile fromwebp-vp8.c || \
        fail_and_exit $?
    run_case_plain "staticcheck-fromwebp-vp8-alpha-strict-compile" \
        run_staticcheck_webp_strict_compile fromwebp-vp8-alpha.c || \
        fail_and_exit $?
    run_case_plain "staticcheck-fromwebp-vp8-parse-strict-compile" \
        run_staticcheck_webp_strict_compile fromwebp-vp8-parse.c || \
        fail_and_exit $?
    run_case_plain "staticcheck-fromwebp-vp8-native-strict-compile" \
        run_staticcheck_webp_strict_compile fromwebp-vp8-native.c || \
        fail_and_exit $?
    run_case_plain "staticcheck-fromwebp-vp8-native-token-strict-compile" \
        run_staticcheck_webp_strict_compile fromwebp-vp8-native-token.c || \
        fail_and_exit $?
    run_case_plain "staticcheck-fromwebp-vp8l-strict-compile" \
        run_staticcheck_webp_strict_compile fromwebp-vp8l.c || \
        fail_and_exit $?
    run_case_plain "staticcheck-fromwebp-trace-strict-compile" \
        run_staticcheck_webp_strict_compile fromwebp-trace.c || \
        fail_and_exit $?

    clang_bin=${CLANG:-clang}
    if tool_is_available "$clang_bin"; then
        run_case_plain "staticcheck-fromwebp-strict-clang-compile" \
            run_staticcheck_webp_strict_compile fromwebp.c "$clang_bin" || \
            fail_and_exit $?
        run_case_plain "staticcheck-fromwebp-vp8-parse-strict-clang-compile" \
            run_staticcheck_webp_strict_compile fromwebp-vp8-parse.c \
            "$clang_bin" || fail_and_exit $?
        run_case_plain "staticcheck-fromwebp-vp8-native-strict-clang-compile" \
            run_staticcheck_webp_strict_compile fromwebp-vp8-native.c \
            "$clang_bin" || fail_and_exit $?
        run_case_plain "staticcheck-fromwebp-vp8-native-token-strict-clang-compile" \
            run_staticcheck_webp_strict_compile fromwebp-vp8-native-token.c \
            "$clang_bin" || fail_and_exit $?
    else
        run_case_skip "staticcheck-fromwebp-strict-clang-compile" \
            "clang not found"
        run_case_skip "staticcheck-fromwebp-vp8-parse-strict-clang-compile" \
            "clang not found"
        run_case_skip "staticcheck-fromwebp-vp8-native-strict-clang-compile" \
            "clang not found"
        run_case_skip "staticcheck-fromwebp-vp8-native-token-strict-clang-compile" \
            "clang not found"
    fi
else
    run_case_skip "staticcheck-threading-no-threads-compile" \
        "missing config.h in build root"
    run_case_skip "staticcheck-timeline-logger-no-threads-symbols" \
        "missing config.h in build root"
    run_case_skip "staticcheck-threadpool-no-threads-symbols" \
        "missing config.h in build root"
    run_case_skip "staticcheck-fromwebp-vp8-tables-self-include" \
        "missing config.h in build root"
    run_case_skip "staticcheck-fromwebp-strict-compile" \
        "missing config.h in build root"
    run_case_skip "staticcheck-fromwebp-container-strict-compile" \
        "missing config.h in build root"
    run_case_skip "staticcheck-fromwebp-vp8-strict-compile" \
        "missing config.h in build root"
    run_case_skip "staticcheck-fromwebp-vp8-alpha-strict-compile" \
        "missing config.h in build root"
    run_case_skip "staticcheck-fromwebp-vp8-parse-strict-compile" \
        "missing config.h in build root"
    run_case_skip "staticcheck-fromwebp-vp8-native-strict-compile" \
        "missing config.h in build root"
    run_case_skip "staticcheck-fromwebp-vp8-native-token-strict-compile" \
        "missing config.h in build root"
    run_case_skip "staticcheck-fromwebp-vp8l-strict-compile" \
        "missing config.h in build root"
    run_case_skip "staticcheck-fromwebp-trace-strict-compile" \
        "missing config.h in build root"
    run_case_skip "staticcheck-fromwebp-strict-clang-compile" \
        "missing config.h in build root"
    run_case_skip "staticcheck-fromwebp-vp8-parse-strict-clang-compile" \
        "missing config.h in build root"
    run_case_skip "staticcheck-fromwebp-vp8-native-strict-clang-compile" \
        "missing config.h in build root"
    run_case_skip "staticcheck-fromwebp-vp8-native-token-strict-clang-compile" \
        "missing config.h in build root"
fi

run_case_tap "staticcheck-build-doc-configure-help" \
    "$src_root/tests/_static/sh/staticcheck-build-doc-configure-help.sh" \
    "$src_root" || fail_and_exit $?

run_case_tap "staticcheck-build-doc-meson-options" \
    "$src_root/tests/_static/sh/staticcheck-build-doc-meson-options.sh" \
    "$src_root" || fail_and_exit $?

run_case_tap "staticcheck-help-quantize-source-contract" \
    "$src_root/tests/_static/sh/staticcheck-help-quantize-source-contract.sh" \
    "$src_root" || fail_and_exit $?

run_case_tap "staticcheck-help-sixel2png-source-contract" \
    "$src_root/tests/_static/sh/staticcheck-help-sixel2png-source-contract.sh" \
    "$src_root" || fail_and_exit $?

run_case_tap "staticcheck-docs-help-vs-man" \
    "$src_root/tests/_static/sh/staticcheck-docs-help-vs-man.sh" \
    "$src_root" || fail_and_exit $?

run_case_tap "staticcheck-docs-man-vs-bash-completion" \
    "$src_root/tests/_static/sh/staticcheck-docs-man-vs-bash-completion.sh" \
    "$src_root" || fail_and_exit $?

run_case_tap "staticcheck-docs-envvars-help-table" \
    "$src_root/tests/_static/sh/staticcheck-docs-envvars-help-table.sh" \
    "$src_root" || fail_and_exit $?

run_case_tap "staticcheck-loader-builtin-1464-static-contract" \
    env ARTIFACT_LOCAL_DIR="$ARTIFACT_ROOT/staticcheck-loader-builtin-1464" \
    TOP_SRCDIR="$TOP_SRCDIR" \
    TOP_BUILDDIR="$TOP_BUILDDIR" \
    sh "$src_root/tests/meson-tap-exitcode-wrapper.sh" \
    "$src_root/tests/loader/builtin/1464_loader_builtin_psd_psdtools_blend_and_clipping_clip_weighted_deferred_solid_overlay_trace.t" || fail_and_exit $?

run_case_tap "staticcheck-loader-builtin-1578-static-contract" \
    env ARTIFACT_LOCAL_DIR="$ARTIFACT_ROOT/staticcheck-loader-builtin-1578" \
    TOP_SRCDIR="$TOP_SRCDIR" \
    TOP_BUILDDIR="$TOP_BUILDDIR" \
    sh "$src_root/tests/meson-tap-exitcode-wrapper.sh" \
    "$src_root/tests/loader/builtin/1578_loader_builtin_psd_psdtools_effects_stroke_composite_vector_stroke_adjust_deferred_trace.t" || fail_and_exit $?

run_case_tap "staticcheck-quant-palette-init-0073-static-contract" \
    env ARTIFACT_LOCAL_DIR="$ARTIFACT_ROOT/staticcheck-quant-palette-init-0073" \
    TOP_SRCDIR="$TOP_SRCDIR" \
    TOP_BUILDDIR="$TOP_BUILDDIR" \
    sh "$src_root/tests/meson-tap-exitcode-wrapper.sh" \
    "$src_root/tests/quant/palette/init/0073_kcenter_auto_perceptual_oklab_hybrid_preference.t" || fail_and_exit $?

run_case_tap "staticcheck-loader-builtin-1609-webp-vp8l-static-contract" \
    env ARTIFACT_LOCAL_DIR="$ARTIFACT_ROOT/staticcheck-loader-builtin-1609" \
    TOP_SRCDIR="$TOP_SRCDIR" \
    TOP_BUILDDIR="$TOP_BUILDDIR" \
    sh "$src_root/tests/loader/builtin/1609_loader_builtin_webp_vp8l_static_decode.t" || fail_and_exit $?

run_case_tap "staticcheck-loader-builtin-1610-webp-vp8-static-contract" \
    env ARTIFACT_LOCAL_DIR="$ARTIFACT_ROOT/staticcheck-loader-builtin-1610" \
    TOP_SRCDIR="$TOP_SRCDIR" \
    TOP_BUILDDIR="$TOP_BUILDDIR" \
    sh "$src_root/tests/loader/builtin/1610_loader_builtin_webp_vp8_static_decode_code.t" || fail_and_exit $?

printf 'staticcheck summary: total=%d pass=%d skip=%d fail=%d\n' \
    "$index" "$pass_count" "$skip_count" "$fail_count"

exit 0
