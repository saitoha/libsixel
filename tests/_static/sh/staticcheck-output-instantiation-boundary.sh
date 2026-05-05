#!/bin/sh
# Emit TAP for legacy output and encoder-core boundary checks.

set -eu

src_root=${1:-}

echo "1..10"

if test -z "$src_root"; then
    echo "not ok 1 - src output construction uses encoder-core factory"
    echo "# src_root argument is required"
    echo "not ok 2 - legacy public output API stays exported"
    echo "# src_root argument is required"
    echo "not ok 3 - 6cells IDL uses encoder_core as canonical name"
    echo "# src_root argument is required"
    echo "not ok 4 - encoder-core classid is registered without terminal aliases"
    echo "# src_root argument is required"
    echo "not ok 5 - legacy emitter projection names stay removed"
    echo "# src_root argument is required"
    echo "not ok 6 - output concrete storage stays encoder-core private"
    echo "# src_root argument is required"
    echo "not ok 7 - sixel_writer storage keeps encode state out"
    echo "# src_root argument is required"
    echo "not ok 8 - encoder_core write method stays removed"
    echo "# src_root argument is required"
    echo "not ok 9 - encoder_core encode is implemented"
    echo "# src_root argument is required"
    echo "not ok 10 - public sixel_encode uses encoder_core vtbl"
    echo "# src_root argument is required"
    exit 1
fi

if test ! -d "$src_root/src"; then
    echo "not ok 1 - src output construction uses encoder-core factory"
    echo "# missing source directory: $src_root/src"
    echo "not ok 2 - legacy public output API stays exported"
    echo "# missing source directory: $src_root/src"
    echo "not ok 3 - 6cells IDL uses encoder_core as canonical name"
    echo "# missing source directory: $src_root/src"
    echo "not ok 4 - encoder-core classid is registered without terminal aliases"
    echo "# missing source directory: $src_root/src"
    echo "not ok 5 - legacy emitter projection names stay removed"
    echo "# missing source directory: $src_root/src"
    echo "not ok 6 - output concrete storage stays encoder-core private"
    echo "# missing source directory: $src_root/src"
    echo "not ok 7 - sixel_writer storage keeps encode state out"
    echo "# missing source directory: $src_root/src"
    echo "not ok 8 - encoder_core write method stays removed"
    echo "# missing source directory: $src_root/src"
    echo "not ok 9 - encoder_core encode is implemented"
    echo "# missing source directory: $src_root/src"
    echo "not ok 10 - public sixel_encode uses encoder_core vtbl"
    echo "# missing source directory: $src_root/src"
    exit 1
fi

tmpdir=$(mktemp -d "${TMPDIR:-/tmp}/libsixel-encoder-core-boundary-XXXXXX")
trap 'rm -rf "$tmpdir"' EXIT HUP INT TERM

idl_file=$src_root/include/6cells.idl
public_header=$src_root/include/sixel.h.in
generated_header=$src_root/include/6cells.h
classid_gperf=$src_root/src/classid-factory.gperf
violations=$tmpdir/direct-output-new.txt
legacy_projection=$tmpdir/legacy-projection.txt
direct_storage=$tmpdir/direct-output-storage.txt
writer_storage=$tmpdir/writer-storage.txt
encoder_write=$tmpdir/encoder-core-write.txt
encoder_notimpl=$tmpdir/encoder-core-not-implemented.txt
public_encode=$tmpdir/public-sixel-encode.txt
failed=0

find "$src_root/src" -type f -name '*.c' ! -name 'output.c' -exec awk '
/sixel_output_new[[:space:]]*\(/ {
    print FILENAME ":" FNR ":" $0
}
' {} + > "$violations"

if test -s "$violations"; then
    echo "not ok 1 - src output construction uses encoder-core factory"
    sed 's/^/# direct constructor: /' "$violations"
    failed=1
else
    echo "ok 1 - src output construction uses encoder-core factory"
fi

if awk '
/typedef struct sixel_output sixel_output_t;/ {
    seen_type = 1
}
/sixel_output_new[[:space:]]*\(/ {
    seen_new = 1
}
/sixel_output_unref[[:space:]]*\(/ {
    seen_unref = 1
}
/sixel_output_set_encode_policy[[:space:]]*\(/ {
    seen_policy = 1
}
END {
    exit (seen_type && seen_new && seen_unref && seen_policy) ? 0 : 1
}
' "$public_header"; then
    echo "ok 2 - legacy public output API stays exported"
else
    echo "not ok 2 - legacy public output API stays exported"
    echo "# missing sixel_output_t or sixel_output_* public API"
    failed=1
fi

if awk '
/^interface[ \t]+(output|sixel_emitter)[ \t]*[;{]/ {
    print "legacy interface remains: " FNR ":" $0
}
/^coclass[ \t]+(output_component|sixel_emitter_component)[ \t]*[{]/ {
    print "legacy coclass remains: " FNR ":" $0
}
/^interface[ \t]+encoder_core[ \t]*[;{]/ {
    seen_interface = 1
}
/^coclass[ \t]+encoder_core_component[ \t]*[{]/ {
    seen_coclass = 1
}
END {
    exit (seen_interface && seen_coclass) ? 0 : 1
}
' "$idl_file" > "$tmpdir/idl.txt"; then
    if test -s "$tmpdir/idl.txt"; then
        echo "not ok 3 - 6cells IDL uses encoder_core as canonical name"
        sed 's/^/# /' "$tmpdir/idl.txt"
        failed=1
    else
        echo "ok 3 - 6cells IDL uses encoder_core as canonical name"
    fi
else
    echo "not ok 3 - 6cells IDL uses encoder_core as canonical name"
    sed 's/^/# /' "$tmpdir/idl.txt"
    failed=1
fi

if awk '
/# define SIXEL_FACTORY_CLASSID_CREATE_[0-9]+ sixel_encoder_core_factory_new/ {
    core_macro[$3] = 1
}
/# define SIXEL_FACTORY_CLASSID_CREATE_[0-9]+ sixel_encoder_core_normal_factory_new/ {
    normal_macro[$3] = 1
}
/# define SIXEL_FACTORY_CLASSID_CREATE_[0-9]+ sixel_encoder_core_highcolor_factory_new/ {
    highcolor_macro[$3] = 1
}
/# define SIXEL_FACTORY_CLASSID_CREATE_[0-9]+ sixel_encoder_core_ormode_factory_new/ {
    ormode_macro[$3] = 1
}
/# define SIXEL_FACTORY_CLASSID_CREATE_[0-9]+ sixel_writer_factory_new/ {
    writer_macro[$3] = 1
}
/^codec\/encoder-core,[ \t]*SIXEL_FACTORY_CLASSID_CREATE_[0-9]+$/ {
    if ($2 in core_macro) {
        seen_canonical = 1
    }
}
/^codec\/encoder-core\.normal,[ \t]*SIXEL_FACTORY_CLASSID_CREATE_[0-9]+$/ {
    if ($2 in normal_macro) {
        seen_normal = 1
    }
}
/^codec\/encoder-core\.highcolor,[ \t]*SIXEL_FACTORY_CLASSID_CREATE_[0-9]+$/ {
    if ($2 in highcolor_macro) {
        seen_highcolor = 1
    }
}
/^codec\/encoder-core\.ormode,[ \t]*SIXEL_FACTORY_CLASSID_CREATE_[0-9]+$/ {
    if ($2 in ormode_macro) {
        seen_ormode = 1
    }
}
/^io\/sixel-writer,[ \t]*SIXEL_FACTORY_CLASSID_CREATE_[0-9]+$/ {
    if ($2 in writer_macro) {
        seen_writer = 1
    }
}
/^terminal\/(output|sixel-emitter),/ {
    print "legacy terminal classid remains: " FNR ":" $0
}
END {
    exit (seen_canonical && seen_normal && seen_highcolor &&
          seen_ormode && seen_writer) ? 0 : 1
}
' "$classid_gperf" > "$tmpdir/classid.txt"; then
    if test -s "$tmpdir/classid.txt"; then
        echo "not ok 4 - encoder-core classid is registered without terminal aliases"
        sed 's/^/# /' "$tmpdir/classid.txt"
        failed=1
    else
        echo "ok 4 - encoder-core classid is registered without terminal aliases"
    fi
else
    echo "not ok 4 - encoder-core classid is registered without terminal aliases"
    echo "# codec/encoder-core variants and io/sixel-writer must resolve"
    sed 's/^/# /' "$tmpdir/classid.txt"
    failed=1
fi

awk '
/sixel_emitter|sixel-emitter|sixel_output_as_emitter/ {
    print FILENAME ":" FNR ":" $0
}
/terminal\/(output|sixel-emitter)/ {
    print FILENAME ":" FNR ":" $0
}
' "$idl_file" "$generated_header" "$classid_gperf" > "$legacy_projection"

if test -s "$legacy_projection"; then
    echo "not ok 5 - legacy emitter projection names stay removed"
    sed 's/^/# legacy projection: /' "$legacy_projection"
    failed=1
else
    echo "ok 5 - legacy emitter projection names stay removed"
fi

find "$src_root/src" -type f \( -name '*.c' -o -name '*.h' \) \
    ! -name 'encoder-core.c' \
    ! -name 'encoder-core-encode.c' \
    ! -name 'encoder-core-highcolor.c' \
    ! -name 'encoder-core-private.h' \
    -exec awk '
BEGIN {
    output_fields = "(ref|allocator|writer|writer_controls|has_8bit_control|has_sixel_scrolling|has_gri_arg_limit|has_sdm_glitch|skip_dcs_envelope|skip_header|palette_type|colorspace|source_colorspace|pixelformat|save_pixel|save_count|active_palette|node_top|node_free|penetrate_multiplexer|encode_policy|ormode|last_frame_time_usec|pos|buffer)"
}
/struct[ \t]+sixel_output[ \t]*\{/ {
    print FILENAME ":" FNR ":" $0
}
/sixel_node_t/ {
    print FILENAME ":" FNR ":" $0
}
{
    if ($0 ~ "(^|[^A-Za-z0-9_])output->[ \t]*" output_fields "([^A-Za-z0-9_]|$)" ||
        $0 ~ "->[ \t]*output->[ \t]*" output_fields "([^A-Za-z0-9_]|$)") {
        print FILENAME ":" FNR ":" $0
    }
}
' {} + > "$direct_storage"

if test -s "$direct_storage"; then
    echo "not ok 6 - output concrete storage stays encoder-core private"
    sed 's/^/# concrete access: /' "$direct_storage"
    failed=1
else
    echo "ok 6 - output concrete storage stays encoder-core private"
fi

awk '
/image_pixels|palette|dither_policy|encode_policy|ormode|gri_arg_limit|body_run_state|sixel_node_t|save_pixel|save_count|active_palette|node_top|node_free/ {
    print FILENAME ":" FNR ":" $0
}
' "$src_root/src/sixel-writer.c" > "$writer_storage"

if test -s "$writer_storage"; then
    echo "not ok 7 - sixel_writer storage keeps encode state out"
    sed 's/^/# writer state: /' "$writer_storage"
    failed=1
else
    echo "ok 7 - sixel_writer storage keeps encode state out"
fi

awk '
function trim(text) {
    gsub(/^[ \t]+/, "", text)
    gsub(/[ \t]+$/, "", text)
    return text
}
{
    line = trim($0)
    if (line ~ /^interface[ \t]+encoder_core[ \t]*\{/) {
        in_encoder_core = 1
    } else if (in_encoder_core && line ~ /^\};?$/) {
        in_encoder_core = 0
    } else if (in_encoder_core &&
               line ~ /^SIXELSTATUS[ \t]+write[ \t]*\(/) {
        print FILENAME ":" FNR ":" $0
    }
}
' "$idl_file" > "$encoder_write"

if test -s "$encoder_write"; then
    echo "not ok 8 - encoder_core write method stays removed"
    sed 's/^/# encoder_core write: /' "$encoder_write"
    failed=1
else
    echo "ok 8 - encoder_core write method stays removed"
fi

awk '
{
    if ($0 ~ /^sixel_encoder_core_vtbl_encode[ \t]*\(/) {
        seen_name = 1
    }
    if (seen_name && $0 ~ /\{/) {
        seen_function = 1
        in_function = 1
        depth = 1
        seen_name = 0
        next
    }
    if (in_function) {
        if ($0 ~ /SIXEL_NOT_IMPLEMENTED/) {
            print FILENAME ":" FNR ":" $0
        }
        line = $0
        open_count = gsub(/\{/, "{", line)
        line = $0
        close_count = gsub(/\}/, "}", line)
        depth += open_count - close_count
        if (depth <= 0) {
            in_function = 0
        }
    }
}
END {
    if (!seen_function) {
        print "sixel_encoder_core_vtbl_encode not found"
    }
}
' "$src_root/src/encoder-core.c" > "$encoder_notimpl"

if test -s "$encoder_notimpl"; then
    echo "not ok 9 - encoder_core encode is implemented"
    sed 's/^/# encoder_core encode: /' "$encoder_notimpl"
    failed=1
else
    echo "ok 9 - encoder_core encode is implemented"
fi

awk '
{
    if ($0 ~ /^sixel_encode[ \t]*\(/) {
        seen_name = 1
    }
    if (seen_name && $0 ~ /\{/) {
        seen_function = 1
        in_function = 1
        depth = 1
        seen_name = 0
        next
    }
    if (in_function) {
        if ($0 ~ /sixel_output_as_encoder_core/) {
            seen_as_encoder_core = 1
        }
        if ($0 ~ /->[ \t]*vtbl->[ \t]*encode/) {
            seen_vtbl_encode = 1
        }
        if ($0 ~ /sixel_encode_(dither|highcolor)[ \t]*\(/) {
            print FILENAME ":" FNR ":direct helper dispatch: " $0
        }
        line = $0
        open_count = gsub(/\{/, "{", line)
        line = $0
        close_count = gsub(/\}/, "}", line)
        depth += open_count - close_count
        if (depth <= 0) {
            in_function = 0
        }
    }
}
END {
    if (!seen_function) {
        print "public sixel_encode not found"
    } else {
        if (!seen_as_encoder_core) {
            print "public sixel_encode does not bind encoder_core"
        }
        if (!seen_vtbl_encode) {
            print "public sixel_encode does not call vtbl encode"
        }
    }
}
' "$src_root/src/encoder-core-encode.c" > "$public_encode"

if test -s "$public_encode"; then
    echo "not ok 10 - public sixel_encode uses encoder_core vtbl"
    sed 's/^/# public encode: /' "$public_encode"
    failed=1
else
    echo "ok 10 - public sixel_encode uses encoder_core vtbl"
fi

exit "$failed"
