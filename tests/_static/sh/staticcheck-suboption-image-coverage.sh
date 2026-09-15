#!/bin/sh
# Exercise the registry image-evidence checker with independent omissions.
# Policy: docs/testing/staticcheck.md
# Mutations below are literal shell source, not values to expand here.
# shellcheck disable=SC2016
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
        perceptual-capture perceptual-reference \
        literal-reference; do
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
        consumer-engine) needle='record#engine=builtin|'; replacement='record#engine=other|' ;;
        consumer-result) needle='record%|ok=1'; replacement='record%|ok=0' ;;
        comparison) needle='cmp -s'; replacement=': -s' ;;
        image-reference) needle='${LSQA_PATH}'; replacement='${OTHER_PATH}' ;;
        legacy-*)
            fixture="$regression_dir/0012_diffusion_stbn_alpha_guard_image_regression.t"
            case "$sample" in
                legacy-assignment) needle='stored=1|binding='; replacement='stored=0|binding=' ;;
                legacy-binding) needle='binding=stbn_alpha_guard_enabled,'; replacement='binding=other_field,' ;;
                legacy-consumer) needle='LSXDTH1|*alpha=1'; replacement='LSXDTH1|*alpha=0' ;;
            esac
            ;;
        perceptual-*)
            fixture="$regression_dir/0082_lookup_5bit_shared_instance_image_regression.t"
            case "$sample" in
                perceptual-pair) needle='"${short_output}" "${env_output}"'; replacement='"${short_output}" "${short_output}"' ;;
                perceptual-capture) needle='equivalence_status=$?'; replacement=': $?' ;;
                perceptual-status) needle='test "${equivalence_status:-0}" -eq 0'; replacement='test "${equivalence_status:-0}" -eq 1' ;;
                perceptual-reference) needle='lsqa_error=$(set +xv; ${SIXEL_RUNTIME-} "${LSQA_PATH}"'; replacement='lsqa_error=$(set +xv; ${SIXEL_RUNTIME-} "${OTHER_PATH}"' ;;
            esac
            ;;
        literal-reference)
            fixture="$regression_dir/0106_loader_builtin_pnm_truncated_ascii_image_regression.t"
            needle='cmp -s "${reference_output}"'; replacement=': -s "${reference_output}"'
            ;;
    esac
    awk -v needle="$needle" -v replacement="$replacement" '
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
