#!/bin/sh
# Verify the source-level OpenVMS compatibility boundaries.
# Policy: docs/misc/platforms/openvms.md
# Coverage: OV-01 OV-02 OV-03 OV-04 OV-05 OV-07 OV-08 OV-09 OV-10 OV-11 OV-12

set -eu

src_root=$1
record_fixture_ledger="$src_root/tests/_static/data/openvms-record-fixtures.tsv"
failed=0

fail()
{
    echo "# $*" >&2
    failed=1
}

require_fixed()
{
    pattern=$1
    path=$2

    grep -F -- "$pattern" "$src_root/$path" >/dev/null 2>&1 ||
        fail "$path is missing: $pattern"
}

echo "1..1"

openvms_detect_line=$(awk '/^sixel_openvms_mode=no$/ { print NR; exit }' \
    "$src_root/configure.ac")
canonical_host_line=$(awk '/^AC_CANONICAL_HOST$/ { print NR; exit }' \
    "$src_root/configure.ac")
test -n "$openvms_detect_line" && test -n "$canonical_host_line" &&
    test "$openvms_detect_line" -lt "$canonical_host_line" ||
    fail "configure must detect OpenVMS before AC_CANONICAL_HOST"

require_fixed \
    "ac_executable_extensions=\".exe \$ac_executable_extensions\"" configure.ac
require_fixed 'ac_check_headers_parallel_max=1' configure.ac
require_fixed 'ac_check_funcs_parallel_max=1' configure.ac
require_fixed 'CPU_COUNT=1' configure.ac
require_fixed 'ac_config_status_jobs=""' configure.ac
require_fixed 'enable_dependency_tracking=no' configure.ac
require_fixed 'openvms/gnv-ar.sh' configure.ac
require_fixed 'openvms/gnv-cc.sh' configure.ac
require_fixed 'openvms/gnv-rm.sh' configure.ac
require_fixed 'if ($[0] !~ /\\$/)' configure.ac

require_fixed 'LS_OPENVMS_BUILD_MAKEFLAGS = -j1 V=1' Makefile.am
require_fixed 'OPENVMS_TEST_JOBS ?= 1' tests/Makefile.am
require_fixed "for obj in \$(libsixel_la_OBJECTS); do" src/Makefile.am

openvms_link_count=$(grep -hF \
    "gnv-link-program.sh -o \$@" \
    "$src_root/converters/Makefile.am" \
    "$src_root/assessment/Makefile.am" \
    "$src_root/tools/Makefile.am" \
    "$src_root/tests/Makefile.am" | awk 'END { print NR }')
test "$openvms_link_count" -eq 6 ||
    fail "expected 6 native OpenVMS program-link overrides, found $openvms_link_count"

/bin/sh "$src_root/openvms/gnv-ar.sh" /bin/sh -c \
    'printf "%s\n" "%LIBRAR-W-COMCOD, warning"; exit 3' >/dev/null ||
    fail "gnv-ar.sh did not accept a warning-only condition"
if /bin/sh "$src_root/openvms/gnv-ar.sh" /bin/sh -c \
    'printf "%s\n" "%LIBRAR-E-FAILED, error"; exit 2' >/dev/null 2>&1; then
    fail "gnv-ar.sh accepted an error condition"
fi

/bin/sh "$src_root/openvms/gnv-cc.sh" /bin/sh -c \
    'printf "%s\n" "%CC-W-WARN, warning"; exit 3' >/dev/null ||
    fail "gnv-cc.sh did not accept a warning-only condition"
if /bin/sh "$src_root/openvms/gnv-cc.sh" /bin/sh -c \
    'printf "%s\n" "error: rejected"; exit 2' >/dev/null 2>&1; then
    fail "gnv-cc.sh accepted an error condition"
fi

missing_path="$src_root/tests/_static/openvms-rm-missing"
false_bin=$(command -v false)
test ! -e "$missing_path" || fail "static-check sentinel unexpectedly exists"
GNV_RM="$false_bin" /bin/sh "$src_root/openvms/gnv-rm.sh" -f \
    "$missing_path" ||
    fail "gnv-rm.sh did not ignore a missing forced-removal operand"

require_fixed '%ILINK-W-NUDFSYMS' openvms/gnv-link-program.sh
require_fixed '%ILINK-W-USEUNDEF' openvms/gnv-link-program.sh
require_fixed "if test -f \"\$out\"; then" openvms/gnv-link-program.sh
require_fixed "die \"unsupported link argument: \$arg\"" \
    openvms/gnv-link-program.sh
if TMPDIR="${TMPDIR:-/tmp}" /bin/sh \
        "$src_root/openvms/gnv-link-program.sh" \
        -o libsixel-openvms-staticcheck.exe unsupported-link-input \
        >/dev/null 2>&1; then
    fail "gnv-link-program.sh accepted an unsupported argument"
fi
if TMPDIR="${TMPDIR:-/tmp}" /bin/sh \
        "$src_root/openvms/gnv-link-program.sh" -o \
        libsixel-openvms-staticcheck.exe >/dev/null 2>&1; then
    fail "gnv-link-program.sh accepted a link with no inputs"
fi

require_fixed '0x10000000' converters/img2sixel.c
require_fixed 'IMG2SIXEL_OPENVMS_INHIBIT_MSG | 2' converters/img2sixel.c
require_fixed 'IMG2SIXEL_OPENVMS_INHIBIT_MSG | 4' converters/img2sixel.c
require_fixed '0x10000000' converters/sixel2png.c
require_fixed 'SIXEL2PNG_OPENVMS_INHIBIT_MSG | 2' converters/sixel2png.c
require_fixed 'SIXEL2PNG_OPENVMS_INHIBIT_MSG | 4' converters/sixel2png.c
require_fixed 'SIXEL_TEST_MAX_MAPPED_ERROR_STATUS=4' \
    build-aux/lso-tap-driver.sh.in
require_fixed 'TEST_RUNNER_OPENVMS_INHIBIT_MSG = 0x10000000' \
    tests/test_runner.c
require_fixed 'return TEST_RUNNER_OPENVMS_INHIBIT_MSG | 2;' \
    tests/test_runner.c
main_unmapped_return_count=$(awk '
    /^main\(int argc, char \*\*argv\)$/ { in_main = 1 }
    in_main && /^[[:space:]]*return / &&
        $0 !~ /test_runner_process_exit_status/ { count++ }
    END { print count + 0 }
' "$src_root/tests/test_runner.c")
test "$main_unmapped_return_count" -eq 0 ||
    fail "test_runner main retains an unmapped OpenVMS process return"
require_fixed "tr -d '\\n' <\"\${out_file}\" | cksum" \
    tests/planner/pipeline/0013_pipeline_gpu_force_reject_closes_dcs.t
require_fixed 'SIXEL_TEST_MAX_MAPPED_ERROR_STATUS-3' \
    tests/security/issue/0012_issue220_dcs_signed_integer_overflow.t
for palette_test in \
    tests/quant/palette/0009_palette_worker_init_fallback.t \
    tests/quant/palette/0010_palette_worker_sample_fallback.t \
    tests/quant/palette/0011_palette_worker_thread_fallback.t \
    tests/quant/palette/0012_palette_worker_conversion_fallback.t \
    tests/quant/palette/0013_palette_worker_build_fallback.t
do
    require_fixed 'tests/data/inputs/snake_64.png' "$palette_test"
    if grep -F 'images/snake.png' "$src_root/$palette_test" >/dev/null 2>&1; then
        fail "$palette_test uses an RMS-hostile full-size output fixture"
    fi
done

require_fixed 'tests/data/inputs/mapfile/* -text' .gitattributes
test -f "$record_fixture_ledger" ||
    fail "OpenVMS record-fixture ledger is missing"
tab=$(printf '\t')
while IFS="$tab" read -r fixture_path fixture_cksum fixture_size; do
    case "$fixture_path" in
        ''|'#'*) continue ;;
    esac
    test -f "$src_root/$fixture_path" || {
        fail "OpenVMS record fixture is missing: $fixture_path"
        continue
    }
    fixture_actual=$(cksum "$src_root/$fixture_path")
    read -r fixture_actual_cksum fixture_actual_size fixture_actual_path <<EOF
$fixture_actual
EOF
    : "$fixture_actual_path"
    test "$fixture_actual_cksum" = "$fixture_cksum" &&
        test "$fixture_actual_size" = "$fixture_size" ||
        fail "OpenVMS record fixture changed: $fixture_path"
    fixture_name=${fixture_path##*/}
    grep -R -F -- "$fixture_name" "$src_root/tests/quant/mapfile" \
        >/dev/null 2>&1 ||
        fail "OpenVMS record fixture is not owned by a mapfile test: $fixture_path"
done < "$record_fixture_ledger"

for mapfile_test in \
    tests/quant/mapfile/0016_mapfile_import_act_accepts_zero_color_count.t \
    tests/quant/mapfile/0080_mapfile_export_act_exact_layout.t \
    tests/quant/mapfile/0081_mapfile_import_act_768_exact.t \
    tests/quant/mapfile/0083_mapfile_import_jasc_lf.t \
    tests/quant/mapfile/0084_mapfile_import_jasc_cr.t \
    tests/quant/mapfile/0085_mapfile_import_jasc_crlf.t \
    tests/quant/mapfile/0086_mapfile_export_riff_exact_layout.t \
    tests/quant/mapfile/0087_mapfile_import_riff_exact_entries.t \
    tests/quant/mapfile/0089_mapfile_import_gpl_lf.t \
    tests/quant/mapfile/0090_mapfile_import_gpl_cr.t \
    tests/quant/mapfile/0091_mapfile_import_gpl_crlf.t \
    tests/quant/mapfile/0093_mapfile_import_pal_rejects_utf32_le_bom.t \
    tests/quant/mapfile/0094_mapfile_import_gpl_rejects_utf32_be_bom.t \
    tests/quant/mapfile/0097_mapfile_import_pal_trims_spaces_and_tabs.t \
    tests/quant/mapfile/0099_mapfile_import_gpl_trims_spaces_and_tabs.t \
    tests/quant/mapfile/0100_mapfile_import_pal_rejects_fourth_numeric_token.t \
    tests/quant/mapfile/0101_mapfile_import_riff_skips_padded_unknown_chunk.t \
    tests/quant/mapfile/0102_mapfile_import_pal_extension_detects_riff.t \
    tests/quant/mapfile/0103_mapfile_import_pal_prefix_detects_riff.t \
    tests/quant/mapfile/0119_mapfile_export_act_stdout_exact.t
do
    require_fixed 'tests/data/inputs/mapfile/' "$mapfile_test"
done
# This is a literal shell fragment asserted in the target test.
# shellcheck disable=SC2016
require_fixed '| cmp - "${expected_palette}"' \
    tests/quant/mapfile/0119_mapfile_export_act_stdout_exact.t
# shellcheck disable=SC2016
if grep -F '>"${actual_palette}"' \
        "$src_root/tests/quant/mapfile/0119_mapfile_export_act_stdout_exact.t" \
        >/dev/null 2>&1; then
    fail "ACT stdout test materializes an RMS-sensitive comparison file"
fi
# These are literal shell fragments asserted in the target tests.
# shellcheck disable=SC2016
require_fixed 'stat -c %s "${actual_palette}"' \
    tests/quant/mapfile/0107_mapfile_export_riff_256_color_fields.t
# shellcheck disable=SC2016
require_fixed 'od -An -tx1 -j20 -N4 "${actual_palette}"' \
    tests/quant/mapfile/0107_mapfile_export_riff_256_color_fields.t
# shellcheck disable=SC2016
require_fixed 'stat -c %s "${actual_palette}"' \
    tests/quant/mapfile/0109_mapfile_export_act_256_color_count.t
# shellcheck disable=SC2016
require_fixed 'od -An -tx1 -j768 -N4 "${actual_palette}"' \
    tests/quant/mapfile/0109_mapfile_export_act_256_color_count.t
# shellcheck disable=SC2016
require_fixed '-o/dev/null "${snake_gray_png}"' \
    tests/quant/palette/usage/0015_grayscale_png_with_palette.t
if grep -F 'target_sixel=' \
        "$src_root/tests/quant/palette/usage/0015_grayscale_png_with_palette.t" \
        >/dev/null 2>&1; then
    fail "grayscale palette smoke test retains an unobserved SIXEL artifact"
fi

test "$failed" -eq 0 || {
    echo "not ok 1 - OpenVMS compatibility boundaries are preserved"
    exit 1
}

echo "ok 1 - OpenVMS compatibility boundaries are preserved"
exit 0
