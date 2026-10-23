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

# The positive control checks global completeness once. Mutations change only
# a test body, so unrelated tests cannot affect their per-file verdicts. Keep
# each mutation in a fresh awk process, with the real registries, to avoid
# leaking parsed flags or assertion counts from the unmodified fixture.
checked_fixtures='|'
for sample in baseline assignment binding base consumer-event consumer-engine \
        consumer-result comparison image-reference legacy-assignment \
        legacy-binding legacy-consumer perceptual-pair perceptual-status \
        perceptual-capture perceptual-reference perceptual-output \
        perceptual-env-output perceptual-append \
        literal-reference presentation-base presentation-outcome \
        presentation-environment presentation-seconds value-nonempty; do
    fixture="$regression_dir/0075_loader_libpng_cms_engine_image_regression.t"
    expected=fail
    needle=
    replacement=
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
            fixture="$regression_dir/0114_diffusion_x_dither_strength_image_regression.t"
            case "$sample" in
                legacy-assignment) needle='stored=1\|binding='; replacement='stored=0\|binding=' ;;
                legacy-binding) needle='binding=x_dither_strength,'; replacement='binding=other_field,' ;;
                legacy-consumer) needle='LSXDTH1\|*diffuse=x_dither\|*x_strength=0.25'; replacement='LSXDTH1\|*diffuse=x_dither\|*x_strength=0.5' ;;
            esac
            ;;
        perceptual-*)
            fixture="$regression_dir/0072_lookup_5bit_shared_instance_image_regression.t"
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
            fixture="$regression_dir/0096_loader_builtin_pnm_truncated_ascii_image_regression.t"
            needle='cmp -s "${reference_output}"'; replacement=': -s "${reference_output}"'
            ;;
        presentation-*)
            fixture="$regression_dir/0166_update_progressive_max_passes_image_regression.t"
            case "$sample" in
                presentation-seconds)
                    fixture="$regression_dir/0167_update_progressive_max_seconds_image_regression.t"
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
    # Every distinct fixture must also pass the focused path unchanged. This
    # prevents an always-failing focused mode from satisfying negative cases.
    case "$checked_fixtures" in
        *"|$fixture|"*) ;;
        *)
            awk -v registry_file="$registry_file" \
                -v completion_registry_file="$completion_file" \
                -v focused_test="$fixture" \
                -f "$src_root/tests/_static/awk/suboption-image-coverage.awk" \
                "$registry_file" "$completion_file" "$fixture" || {
                echo 'not ok 1 - unchanged image fixture passes focused inspection'
                exit 0
            }
            checked_fixtures="$checked_fixtures$fixture|"
            ;;
    esac
    set -- "$registry_file" "$completion_file"
    focused_test="$fixture"
    case "$sample" in
        baseline)
            focused_test=
            set -- "$@" "$regression_dir"/*.t
            ;;
        *) set -- "$@" "$fixture" ;;
    esac
    status=0
    SIXEL_IMAGE_MUTATION_NEEDLE="$needle" \
        SIXEL_IMAGE_MUTATION_REPLACEMENT="$replacement" \
        awk -v registry_file="$registry_file" \
            -v completion_registry_file="$completion_file" \
            -v focused_test="$focused_test" \
            -f "$src_root/tests/_static/awk/suboption-image-coverage.awk" \
            "$@" > "$tmpdir/diagnostics" || status=$?
    case "$status" in
        0) actual=pass ;;
        1) actual=fail ;;
        *)
            echo 'not ok 1 - image-evidence mutation executes successfully'
            printf '# case %s exited with status %s\n' "$sample" "$status"
            cat "$tmpdir/diagnostics"
            exit 0
            ;;
    esac
    test "$actual" = "$expected" || {
        echo 'not ok 1 - registry image evidence controls'
        printf '# case %s expected %s, got %s\n' "$sample" "$expected" "$actual"
        cat "$tmpdir/diagnostics"
        exit 0
    }
done

# Focused inspection must reject missing, empty and extra inputs rather than
# treating an incomplete invocation as a passing positive control.
for scope in empty missing extra extra-empty unmatched; do
    focused_test="$fixture"
    needle=
    expected_status=1
    set -- "$registry_file" "$completion_file" "$fixture"
    case "$scope" in
        empty)
            : > "$tmpdir/changed.t"
            focused_test="$tmpdir/changed.t"
            set -- "$registry_file" "$completion_file" "$focused_test"
            ;;
        missing) focused_test="$tmpdir/missing.t" ;;
        extra)
            set -- "$@" \
                "$regression_dir/0075_loader_libpng_cms_engine_image_regression.t"
            ;;
        extra-empty) set -- "$@" "$tmpdir/changed.t" ;;
        unmatched)
            needle=__missing_image_evidence_mutation__
            expected_status=2
            ;;
    esac
    status=0
    SIXEL_IMAGE_MUTATION_NEEDLE="$needle" \
        SIXEL_IMAGE_MUTATION_REPLACEMENT='' \
        awk -v registry_file="$registry_file" \
            -v completion_registry_file="$completion_file" \
            -v focused_test="$focused_test" \
            -f "$src_root/tests/_static/awk/suboption-image-coverage.awk" \
            "$@" > "$tmpdir/diagnostics" || status=$?
    test "$status" -eq "$expected_status" || {
        echo 'not ok 1 - focused image inspection rejects incomplete scope'
        printf '# case %s expected status %s, got %s\n' \
            "$scope" "$expected_status" "$status"
        cat "$tmpdir/diagnostics"
        exit 0
    }
done

echo 'ok 1 - registry image evidence detects missing assignment and consumers'
