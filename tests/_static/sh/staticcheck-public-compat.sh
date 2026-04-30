#!/bin/sh
# Emit TAP for v1.8.7 public source and binary compatibility checks.

set -eu

src_root=${1:-}
build_root=${2:-${TOP_BUILDDIR:-$src_root}}
cc_bin=${3:-${CC:-cc}}

echo "1..3"

if test -z "$src_root"; then
    echo "not ok 1 - v1.8.7 public header symbols remain declared"
    echo "# src_root argument is required"
    echo "not ok 2 - frame public source contract remains compatible"
    echo "# src_root argument is required"
    echo "not ok 3 - v1.8.7 public ABI symbols keep source exports"
    echo "# src_root argument is required"
    exit 1
fi

header=$src_root/include/sixel.h.in
frame_header=$src_root/src/frame.h
private_header=$src_root/src/frame-private.h
frame_impl=$src_root/src/frame.c
generated_header=$build_root/include/sixel.h
failed=0

tmpdir=$(mktemp -d "${TMPDIR:-/tmp}/libsixel-public-compat-XXXXXX")
trap 'rm -rf "$tmpdir"' EXIT HUP INT TERM

expected_symbols=$tmpdir/v1.8.7-public-symbols.txt
current_symbols=$tmpdir/current-public-symbols.txt
source_symbols=$tmpdir/source-public-symbols.txt
missing_symbols=$tmpdir/missing-public-symbols.txt
missing_exports=$tmpdir/missing-source-exports.txt
forbidden_interface=$tmpdir/forbidden-interface.txt
forbidden_frame_internal=$tmpdir/forbidden-frame-internal.txt

cat > "$expected_symbols" <<'EOF'
sixel_allocator_calloc
sixel_allocator_free
sixel_allocator_malloc
sixel_allocator_new
sixel_allocator_realloc
sixel_allocator_ref
sixel_allocator_unref
sixel_decode
sixel_decode_raw
sixel_decoder_create
sixel_decoder_decode
sixel_decoder_new
sixel_decoder_ref
sixel_decoder_setopt
sixel_decoder_unref
sixel_dither_create
sixel_dither_destroy
sixel_dither_get
sixel_dither_get_num_of_histgram_colors
sixel_dither_get_num_of_histogram_colors
sixel_dither_get_num_of_palette_colors
sixel_dither_get_palette
sixel_dither_initialize
sixel_dither_new
sixel_dither_ref
sixel_dither_set_body_only
sixel_dither_set_complexion_score
sixel_dither_set_diffusion_type
sixel_dither_set_optimize_palette
sixel_dither_set_palette
sixel_dither_set_pixelformat
sixel_dither_set_transparent
sixel_dither_unref
sixel_encode
sixel_encoder_create
sixel_encoder_encode
sixel_encoder_encode_bytes
sixel_encoder_new
sixel_encoder_ref
sixel_encoder_set_cancel_flag
sixel_encoder_setopt
sixel_encoder_unref
sixel_frame_clip
sixel_frame_create
sixel_frame_get_delay
sixel_frame_get_frame_no
sixel_frame_get_height
sixel_frame_get_loop_no
sixel_frame_get_multiframe
sixel_frame_get_ncolors
sixel_frame_get_palette
sixel_frame_get_pixelformat
sixel_frame_get_pixels
sixel_frame_get_transparent
sixel_frame_get_width
sixel_frame_init
sixel_frame_new
sixel_frame_ref
sixel_frame_resize
sixel_frame_strip_alpha
sixel_frame_unref
sixel_helper_compute_depth
sixel_helper_format_error
sixel_helper_get_additional_message
sixel_helper_load_image_file
sixel_helper_normalize_pixelformat
sixel_helper_scale_image
sixel_helper_set_additional_message
sixel_helper_write_image_file
sixel_output_create
sixel_output_destroy
sixel_output_get_8bit_availability
sixel_output_new
sixel_output_ref
sixel_output_set_8bit_availability
sixel_output_set_encode_policy
sixel_output_set_gri_arg_limit
sixel_output_set_palette_type
sixel_output_set_penetrate_multiplexer
sixel_output_set_skip_dcs_envelope
sixel_output_unref
EOF

if test ! -f "$header"; then
    echo "not ok 1 - v1.8.7 public header symbols remain declared"
    echo "# missing header: $header"
    failed=1
else
    awk '
    /^sixel_[A-Za-z0-9_]+[[:space:]]*\(/ {
        name = $1
        sub(/\(.*/, "", name)
        print name
    }
    ' "$header" | LC_ALL=C sort -u > "$current_symbols"

    comm -23 "$expected_symbols" "$current_symbols" > "$missing_symbols"
    if test -s "$missing_symbols"; then
        echo "not ok 1 - v1.8.7 public header symbols remain declared"
        sed 's/^/# missing declaration: /' "$missing_symbols"
        failed=1
    elif ! awk '
        /@attr_func_deprecated@/ {
            deprecated_line = 1
            next
        }
        /^sixel_dither_set_complexion_score[[:space:]]*\(/ {
            if (deprecated_line == 1) {
                found = 1
            } else {
                unmarked = 1
            }
        }
        {
            deprecated_line = 0
        }
        END {
            if (found == 1 && unmarked != 1) {
                exit 0
            }
            exit 1
        }
        ' "$header"; then
        echo "not ok 1 - v1.8.7 public header symbols remain declared"
        echo "# sixel_dither_set_complexion_score is not deprecated in sixel.h.in"
        failed=1
    else
        echo "ok 1 - v1.8.7 public header symbols remain declared"
    fi
fi

if test ! -f "$header" || test ! -f "$frame_header" ||
        test ! -f "$private_header" ||
        test ! -f "$frame_impl"; then
    echo "not ok 2 - frame public source contract remains compatible"
    echo "# missing frame header input"
    failed=1
else
    if awk '
        /^struct sixel_frame;$/ {
            public_tag = 1
        }
        /^typedef struct sixel_frame sixel_frame_t;$/ {
            public_typedef = 1
        }
        /^struct sixel_frame_interface;$/ {
            public_interface_tag = 1
        }
        END {
            if (public_tag == 1 && public_typedef == 1 &&
                    public_interface_tag != 1) {
                exit 0
            }
            exit 1
        }
        ' "$header" &&
            awk '
            /^typedef struct sixel_frame_interface sixel_frame_interface_t;$/ {
                frame_interface_typedef = 1
            }
            /^struct sixel_frame_interface[[:space:]]*\{$/ {
                frame_interface_tag = 1
            }
            /sixel_frame_vtbl_t const \*vtbl;/ {
                frame_interface_vtbl = 1
            }
            END {
                if (frame_interface_typedef == 1 &&
                    frame_interface_tag == 1 &&
                    frame_interface_vtbl == 1) {
                    exit 0
                }
                exit 1
            }
            ' "$frame_header" &&
            awk '
            /^SIXEL_INTERNAL_API/ {
                if ($0 ~ /sixel_[A-Za-z0-9_]+[[:space:]]*\(/ &&
                    $0 !~ /sixel_frame_factory_new[[:space:]]*\(/) {
                    print FNR ":" $0
                    found = 1
                }
                pending = 1
                next
            }
            pending == 1 &&
                    /^sixel_frame_factory_new[[:space:]]*\(/ {
                pending = 0
                next
            }
            pending == 1 &&
                    /^sixel_[A-Za-z0-9_]+[[:space:]]*\(/ {
                print FNR ":" $0
                found = 1
                pending = 0
                next
            }
            pending == 1 && /^$/ {
                next
            }
            pending == 1 && /^[^[:space:]]/ {
                pending = 0
            }
            END {
                if (found == 1) {
                    exit 1
                }
                exit 0
            }
            ' "$frame_header" > "$forbidden_frame_internal" &&
            awk '
            /^struct sixel_frame[[:space:]]*\{$/ {
                storage_tag = 1
            }
            /sixel_frame_interface_t frame_interface;/ {
                storage_frame_interface = 1
            }
            END {
                if (storage_tag == 1 && storage_frame_interface == 1) {
                    exit 0
                }
                exit 1
            }
            ' "$private_header" &&
            awk '
            # Windows rpc.h defines interface as a macro. Keep frame.c free
            # of the standalone token so internal parameter names cannot be
            # rewritten under MSYS2/MinGW header combinations.
            /(^|[^A-Za-z0-9_])interface([^A-Za-z0-9_]|$)/ {
                print FNR ":" $0
                found = 1
            }
            END {
                if (found == 1) {
                    exit 1
                }
                exit 0
            }
            ' "$frame_impl" > "$forbidden_interface"; then
        if command -v "$cc_bin" >/dev/null 2>&1 &&
                test -f "$generated_header"; then
            compile_input=$tmpdir/source-compat.c
            cat > "$compile_input" <<'EOF'
#include <sixel.h>

int
main(void)
{
    struct sixel_frame *frame = 0;

    sixel_frame_ref(frame);
    sixel_dither_set_complexion_score(0, 1);
    return 0;
}
EOF
            if "$cc_bin" \
                    -I"$build_root/include" \
                    -Werror=incompatible-pointer-types \
                    -Werror=implicit-function-declaration \
                    -Wno-deprecated-declarations \
                    -c "$compile_input" \
                    -o "$tmpdir/source-compat.o" \
                    > "$tmpdir/source-compat.log" 2>&1; then
                echo "ok 2 - frame public source contract remains compatible"
            else
                echo "not ok 2 - frame public source contract remains compatible"
                sed 's/^/# /' "$tmpdir/source-compat.log"
                failed=1
            fi
        else
            echo "ok 2 - frame public source contract remains compatible"
            echo "# compile probe skipped: compiler or generated sixel.h unavailable"
        fi
    else
        echo "not ok 2 - frame public source contract remains compatible"
        if test -s "$forbidden_interface"; then
            sed 's/^/# macro-sensitive identifier: /' \
                "$forbidden_interface"
        elif test -s "$forbidden_frame_internal"; then
            sed 's/^/# unexpected frame INTERNAL_API: /' \
                "$forbidden_frame_internal"
        else
            echo "# expected public frame handle and internal IFrame header"
        fi
        failed=1
    fi
fi

if test ! -d "$src_root/src"; then
    echo "not ok 3 - v1.8.7 public ABI symbols keep source exports"
    echo "# missing source directory: $src_root/src"
    failed=1
else
    find "$src_root/src" -type f -name '*.c' -exec awk '
    FNR == 1 {
        pending = 0
        candidate = ""
        scan = 0
    }
    /^SIXELAPI([[:space:]]|$)/ {
        pending = 8
        next
    }
    pending > 0 && /^sixel_[A-Za-z0-9_]+[[:space:]]*\(/ {
        name = $1
        sub(/\(.*/, "", name)
        candidate = name
        scan = 128
        pending = 0
        if ($0 ~ /\{/) {
            print candidate
            candidate = ""
            scan = 0
        } else if ($0 ~ /\);[[:space:]]*$/) {
            candidate = ""
            scan = 0
        }
        next
    }
    candidate != "" {
        if ($0 ~ /\{/) {
            print candidate
            candidate = ""
            scan = 0
            next
        }
        if ($0 ~ /\);[[:space:]]*$/) {
            candidate = ""
            scan = 0
            next
        }
        scan--
        if (scan <= 0) {
            candidate = ""
        }
        next
    }
    pending > 0 {
        pending--
    }
    ' {} + | LC_ALL=C sort -u > "$source_symbols"

    comm -23 "$expected_symbols" "$source_symbols" > "$missing_exports"
    if test -s "$missing_exports"; then
        echo "not ok 3 - v1.8.7 public ABI symbols keep source exports"
        sed 's/^/# missing source export: /' "$missing_exports"
        failed=1
    else
        echo "ok 3 - v1.8.7 public ABI symbols keep source exports"
    fi
fi

exit "$failed"
