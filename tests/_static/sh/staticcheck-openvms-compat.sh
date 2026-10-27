#!/bin/sh
# Verify the source-level OpenVMS compatibility boundaries.
# Policy: docs/misc/platforms/openvms.md
# Coverage: OV-01 OV-02 OV-03 OV-04 OV-05 OV-07 OV-08 OV-09 OV-10 OV-11 OV-12 OV-14 OV-17
# Coverage: OV-20

# Named tests form the OpenVMS compatibility ledger: each records a specific
# RMS record, native exit-status, or GNV I/O workaround, not suite coverage.

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

    test -f "$src_root/$path" || {
        fail "missing compatibility file: $path"
        return
    }
    grep -F -- "$pattern" "$src_root/$path" >/dev/null 2>&1 ||
        fail "$path is missing: $pattern"
}

echo "1..1"

# Inspect maintained sources, including TAP, Make and Meson inputs. The
# scanner joins continued lines and ignores comments, but does not attempt
# data-flow analysis of dynamically constructed filenames.
stream_scanner="$src_root/tests/_static/awk/native-stream-paths.awk"
awk -v selftest=1 -f "$stream_scanner" </dev/null ||
    fail "native stream path scanner self-test failed"
find "$src_root/src" "$src_root/include" "$src_root/converters" \
    "$src_root/assessment" "$src_root/tools" "$src_root/tests" \
    "$src_root/build-aux" "$src_root/examples" "$src_root/openvms" \
    -type f \( -name '*.c' -o -name '*.h' -o -name '*.cc' \
    -o -name '*.cpp' -o -name '*.m' -o -name '*.t' -o -name '*.sh' \
    -o -name '*.bash' -o -name '*.am' -o -name '*.mk' -o -name '*.in' \
    -o -name 'meson.build' -o -name 'Makefile' \) \
    -exec awk -f "$stream_scanner" {} + ||
    fail "native file paths must not depend on Unix stream aliases"
awk -f "$stream_scanner" "$src_root/Makefile.am" \
    "$src_root/meson.build" "$src_root/configure.ac" ||
    fail "top-level build inputs use a native stream alias"

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
test "$openvms_link_count" -eq 7 ||
    fail "expected 7 native OpenVMS program-link overrides, found $openvms_link_count"
# The developer measurement tool must use the same native linker path.
# shellcheck disable=SC2016
require_fixed 'progressive_study_LINK = $(SHELL) $(top_srcdir)/openvms/gnv-link-program.sh -o $@' \
    tools/Makefile.am

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

# Exercise the actual emitted wrapper, including GNV's native-image exec
# behavior: exec can run the image and return to the remaining shell source.
# A comparison must run exactly once and preserve all three cmp outcomes.
cmp_wrapper=$(awk '
    $0 == "top_builddir=${TOP_BUILDDIR-}" { copying = 1 }
    copying && $0 == "EOF" { exit }
    copying {
        gsub(/exec "\$real_cmp"/, "gnv_exec \"$real_cmp\"")
        print
    }
' "$src_root/build-aux/lso-tap-driver.sh.in")
test -n "$cmp_wrapper" || fail "OpenVMS cmp wrapper body is missing"

check_cmp_wrapper()
{
    cmp_expected=$1
    cmp_expected_status=$2
    shift 2
    cmp_status=0
    cmp_actual=$(TOP_BUILDDIR="$src_root" \
        CMP_PROBE_STATUS="$cmp_expected_status" /bin/sh -c '
        gnv_exec() { "$@"; }
        probe_cmp() {
            printf "%s\n" "$PWD" "$@"
            return "$CMP_PROBE_STATUS"
        }
        real_cmp=probe_cmp
    '"$cmp_wrapper" sh "$@") || cmp_status=$?
    test "$cmp_status" -eq "$cmp_expected_status" ||
        fail "OpenVMS cmp wrapper changed status $cmp_expected_status"
    test "$cmp_actual" = "$cmp_expected" ||
        fail "OpenVMS cmp wrapper repeated a comparison or changed operands"
}

# Autotools can retain /build/.. in abs_top_srcdir.  cd normalizes PWD, so
# resolve the expected directory while still probing the original operands.
cmp_absolute_dir=$(CDPATH='' cd "$src_root/tests" && pwd)
for cmp_probe_status in 0 1 2; do
    cmp_relative=$(printf '%s\n' "$PWD" -s 'first file' 'second file')
    check_cmp_wrapper "$cmp_relative" "$cmp_probe_status" \
        -s 'first file' 'second file'
    cmp_absolute=$(printf '%s\n' "$cmp_absolute_dir" \
        -s 'first file' '../second file')
    check_cmp_wrapper "$cmp_absolute" "$cmp_probe_status" \
        -s "$src_root/tests/first file" "$src_root/second file"
done

require_fixed 'extern void __posix_exit(int);' \
    tests/test_runner.c
require_fixed '__posix_exit(status);' \
    tests/test_runner.c
main_unmapped_return_count=$(awk '
    /^main\(int argc, char \*\*argv\)$/ { in_main = 1 }
    in_main && /^[[:space:]]*return / &&
        $0 !~ /test_runner_process_exit_status/ { count++ }
    END { print count + 0 }
' "$src_root/tests/test_runner.c")
test "$main_unmapped_return_count" -eq 0 ||
    fail "test_runner main retains an unmapped OpenVMS process return"
require_fixed "tr -d '\\n' <\"\${out_file}\" | \"\${CKSUM:-cksum}\"" \
    tests/planner/pipeline/0013_pipeline_gpu_force_reject_closes_dcs.t
require_fixed 'SIXEL_TEST_MAX_MAPPED_ERROR_STATUS-3' \
    tests/security/issue/0012_issue220_dcs_signed_integer_overflow.t
for retry_test in \
    tests/cli/options/matching/0231_option_matching_palette_exact_float_retry_rejected.t \
    tests/cli/options/matching/0234_option_matching_palette_kcenter_exact_retry_rejected.t
do
    require_fixed 'SIXEL_TEST_MAX_MAPPED_ERROR_STATUS-3' "$retry_test"
done
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
# Captures used by image assertions must retain every byte even when a
# SIXEL packet exceeds the GNV shell-created RMS record-size limit.
for capture_test in \
    tests/cli/options/regression/0072_lookup_5bit_shared_instance_image_regression.t \
    tests/cli/options/regression/0073_lookup_6bit_shared_instance_image_regression.t \
    tests/cli/options/regression/0179_threading_load_image_regression.t \
    tests/cli/options/regression/0183_threading_prepare_workers_image_regression.t
do
    # Match shell source, not values from this checker environment.
    # shellcheck disable=SC2016
    require_fixed '-o "${short_output}"' "$capture_test"
    # shellcheck disable=SC2016
    require_fixed '-o "${env_output}"' "$capture_test"
done
for capture_test in \
    tests/loader/builtin/0001_lsqa_format_grayscale.t \
    tests/loader/builtin/0003_lsqa_format_rgb.t \
    tests/loader/builtin/0115_loader_builtin_jpeg_ycbcr_12bit_seq420_r0_expected_lsqa.t \
    tests/loader/builtin/0119_loader_builtin_jpeg_rgb_8bit_seq444_r0_expected_lsqa.t \
    tests/loader/builtin/0120_loader_builtin_jpeg_ycbcr_8bit_seq444_r0_expected_lsqa.t \
    tests/loader/builtin/0121_loader_builtin_jpeg_ycbcr_8bit_prog420_r0_expected_lsqa.t \
    tests/loader/builtin/0122_loader_builtin_jpeg_cmyk_8bit_seq444_r0_expected_lsqa.t \
    tests/loader/builtin/0123_loader_builtin_jpeg_ycck_8bit_seq444_r0_expected_lsqa.t \
    tests/loader/builtin/0124_loader_builtin_jpeg_embedded_esrgb_cms0_expected_lsqa.t \
    tests/loader/builtin/0125_loader_builtin_jpeg_embedded_esrgb_nolcms_cms1_expected_lsqa.t \
    tests/loader/builtin/0128_loader_builtin_jpeg_cmyk_12bit_seq444_r0_expected_lsqa.t \
    tests/loader/builtin/0130_loader_builtin_jpeg_ycck_12bit_seq444_r0_expected_lsqa.t
do
    # shellcheck disable=SC2016
    require_fixed '-o "${output_sixel}"' "$capture_test"
done
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
    fixture_actual=$("${CKSUM:-cksum}" "$src_root/$fixture_path")
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
