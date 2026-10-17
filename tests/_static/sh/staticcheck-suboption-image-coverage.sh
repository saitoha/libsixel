#!/bin/sh
# Exercise the registry image-evidence checker with independent omissions.
# Policy: docs/testing/staticcheck.md
# Mutations below are literal shell source, not values to expand here.
# shellcheck disable=SC2016
# Named tests are mutation fixtures for this checker itself. Their exact
# assertion syntax is the input under test, not a behavioral coverage ledger.

set -eu

echo '1..1'
src_root=$1
registry_file="$src_root/src/options-registry.c"
completion_file="$src_root/converters/completion_utils.c"
regression_dir="$src_root/tests/cli/options/regression"
tmpdir=$(mktemp -d "${TMPDIR:-/tmp}/libsixel-image-coverage-XXXXXX")
trap 'rm -rf "$tmpdir"' EXIT HUP INT TERM

for sample in baseline assignment binding base consumer-event consumer-engine \
        consumer-result comparison image-reference legacy-assignment \
        legacy-binding legacy-consumer perceptual-pair perceptual-status \
        perceptual-capture perceptual-reference perceptual-output \
        perceptual-env-output perceptual-append \
        literal-reference presentation-base presentation-outcome \
        presentation-environment presentation-seconds value-nonempty; do
    fixture="$regression_dir/0085_loader_libpng_cms_engine_image_regression.t"
    expected=fail
    needle=__unchanged_positive_control__
    replacement=__unused__
    case "$sample" in
        baseline) expected=pass ;;
        assignment) needle='stored=1|binding='; replacement='stored=0|binding=' ;;
        binding) needle='binding=libpng_cms_engine|'; replacement='binding=other_field|' ;;
        base) needle='schema=9|base=libpng|'; replacement='schema=9|base=libjpeg|' ;;
        consumer-event) needle='LSXCMS2|event=apply|'; replacement='LSXCMS2|event=select|' ;;
        consumer-engine) needle='engine=builtin\|*)'; replacement='engine=other\|*)' ;;
        consumer-result) needle='*\|ok=1)'; replacement='*\|ok=0)' ;;
        comparison) needle='cmp -s'; replacement=': -s' ;;
        image-reference) needle='${TEST_RUNNER_PATH}'; replacement='${OTHER_PATH}' ;;
        legacy-*)
            fixture="$regression_dir/0012_diffusion_stbn_alpha_guard_image_regression.t"
            case "$sample" in
                legacy-assignment) needle='stored=1\|binding='; replacement='stored=0\|binding=' ;;
                legacy-binding) needle='binding=stbn_alpha_guard_enabled,'; replacement='binding=other_field,' ;;
                legacy-consumer) needle='LSXDTH1\|*alpha=1'; replacement='LSXDTH1\|*alpha=0' ;;
            esac
            ;;
        perceptual-*)
            fixture="$regression_dir/0082_lookup_5bit_shared_instance_image_regression.t"
            case "$sample" in
                perceptual-pair) needle='"${short_output}" "${env_output}"'; replacement='"${short_output}" "${short_output}"' ;;
                perceptual-capture) needle='equivalence_status=$?'; replacement=': $?' ;;
                perceptual-status) needle='test "${equivalence_status:-0}" -eq 0'; replacement='test "${equivalence_status:-0}" -eq 1' ;;
                perceptual-reference) needle='lsqa_error=$(set +xv; ${SIXEL_RUNTIME-} "${LSQA_PATH}"'; replacement='lsqa_error=$(set +xv; ${SIXEL_RUNTIME-} "${OTHER_PATH}"' ;;
                perceptual-output) needle='-o "${short_output}"'; replacement='' ;;
                perceptual-env-output) needle='-o "${env_output}"'; replacement='' ;;
                perceptual-append) needle='-o "${short_output}"'; replacement='>>"${short_output}"' ;;
            esac
            ;;
        value-nonempty)
            fixture="$regression_dir/0003_diffusion_fs_scan_image_regression.t"
            needle='test -s "${short_output}"'; replacement=': "${short_output}"'
            ;;
        literal-reference)
            fixture="$regression_dir/0106_loader_builtin_pnm_truncated_ascii_image_regression.t"
            needle='cmp -s "${reference_output}"'; replacement=': -s "${reference_output}"'
            ;;
        presentation-*)
            fixture="$regression_dir/0176_update_progressive_max_passes_image_regression.t"
            case "$sample" in
                presentation-seconds)
                    fixture="$regression_dir/0177_update_progressive_max_seconds_image_regression.t"
                    needle='*"max_seconds=1e-300"*'; replacement='*"max_seconds=0"*' ;;
                presentation-base) needle='-Z delta:P1'; replacement='-Z progressive' ;;
                presentation-outcome) needle='progressive: stop=max_passes'; replacement='progressive: stop=converged' ;;
                presentation-environment) needle='case "${delta_env_trace}" in *"progressive:'; replacement='case "${delta_short_trace}" in *"progressive:' ;;
            esac
            ;;
    esac
    test -f "$fixture" || {
        echo "not ok 1 - registry image-evidence fixtures exist"
        echo "# missing checker fixture: $fixture"
        exit 1
    }
    PATTERN_NEEDLE="$needle" PATTERN_REPLACEMENT="$replacement" awk '
    BEGIN { needle = ENVIRON["PATTERN_NEEDLE"]; replacement = ENVIRON["PATTERN_REPLACEMENT"] }
    {
        offset = index($0, needle)
        if (offset) {
            $0 = substr($0, 1, offset - 1) replacement \
                substr($0, offset + length(needle))
        }
        print
    }
    ' "$fixture" > "$tmpdir/changed.t"
    set -- "$registry_file" "$completion_file"
    for input in "$regression_dir"/*.t; do
        case "$input" in
            "$fixture") set -- "$@" "$tmpdir/changed.t" ;;
            *) set -- "$@" "$input" ;;
        esac
    done
    if awk -v registry_file="$registry_file" \
            -v completion_registry_file="$completion_file" \
            -f "$src_root/tests/_static/awk/suboption-image-coverage.awk" \
            "$@" > "$tmpdir/diagnostics"; then
        actual=pass
    else
        actual=fail
    fi
    test "$actual" = "$expected" || {
        echo 'not ok 1 - registry image evidence controls'
        printf '# case %s expected %s, got %s\n' "$sample" "$expected" "$actual"
        cat "$tmpdir/diagnostics"
        exit 0
    }
done

echo 'ok 1 - registry image evidence detects missing assignment and consumers'
