#!/usr/bin/env python3
"""Generate static minimal PSD fixtures for policy/trace tests.

This script is for fixture regeneration only.
It is never invoked from TAP tests.
"""

import pathlib
import struct


def read_u32be(data: bytes, offset: int) -> int:
    return struct.unpack(">I", data[offset : offset + 4])[0]


def read_u16be(data: bytes, offset: int) -> int:
    return struct.unpack(">H", data[offset : offset + 2])[0]


def write_u16be(buf: bytearray, offset: int, value: int) -> None:
    buf[offset : offset + 2] = struct.pack(">H", value & 0xFFFF)


def write_u32be(buf: bytearray, offset: int, value: int) -> None:
    buf[offset : offset + 4] = struct.pack(">I", value & 0xFFFFFFFF)


def locate_offsets(data: bytes) -> tuple[int, int, int]:
    """Return (layer_length_field_offset, compression_offset, image_data_offset)."""
    offset = 26

    color_mode_len = read_u32be(data, offset)
    offset += 4 + color_mode_len

    image_resources_len = read_u32be(data, offset)
    offset += 4 + image_resources_len

    layer_length_field_offset = offset
    layer_mask_len = read_u32be(data, layer_length_field_offset)
    offset += 4 + layer_mask_len

    compression_offset = offset
    image_data_offset = compression_offset + 2
    return layer_length_field_offset, compression_offset, image_data_offset


def convert_psd_minimal_to_psb(data: bytes, *, convert_rle_row_table: bool) -> bytes:
    """Convert minimal PSD bytes to PSB bytes (version=2)."""
    layer_len_off, compression_off, image_data_off = locate_offsets(data)
    layer_mask_len = read_u32be(data, layer_len_off)
    channels = read_u16be(data, 12)
    height = read_u32be(data, 14)
    compression = read_u16be(data, compression_off)
    out = bytearray()

    out += data[:layer_len_off]
    out += struct.pack(">Q", layer_mask_len)
    out += data[layer_len_off + 4 : compression_off]
    out += data[compression_off : compression_off + 2]
    if convert_rle_row_table and compression == 1:
        table_entries = channels * height
        table_bytes = table_entries * 2
        old_table = data[image_data_off : image_data_off + table_bytes]
        if len(old_table) != table_bytes:
            raise RuntimeError("invalid PSD RLE row table")
        for i in range(table_entries):
            row_length = read_u16be(old_table, i * 2)
            out += struct.pack(">I", row_length)
        out += data[image_data_off + table_bytes :]
    else:
        out += data[image_data_off:]
    write_u16be(out, 4, 2)
    return bytes(out)


def locate_image_resources(data: bytes) -> tuple[int, int, int]:
    """Return (image_resources_length_offset, resources_data_offset, resources_end)."""
    offset = 26
    color_mode_len = read_u32be(data, offset)
    offset += 4 + color_mode_len
    image_resources_length_offset = offset
    image_resources_len = read_u32be(data, image_resources_length_offset)
    resources_data_offset = image_resources_length_offset + 4
    resources_end = resources_data_offset + image_resources_len
    if resources_end > len(data):
        raise RuntimeError("invalid image resources section")
    return image_resources_length_offset, resources_data_offset, resources_end


def extract_icc_resource_block(data: bytes) -> bytes:
    """Extract the full ICC image-resource block (including header and padding)."""
    _, cursor, end = locate_image_resources(data)

    while cursor + 12 <= end:
        block_start = cursor
        if data[cursor : cursor + 4] != b"8BIM":
            break
        resource_id = read_u16be(data, cursor + 4)
        cursor += 6

        name_length = data[cursor]
        cursor += 1 + name_length
        if cursor & 1:
            cursor += 1
        if cursor + 4 > end:
            break

        resource_size = read_u32be(data, cursor)
        cursor += 4
        block_end = cursor + resource_size
        if resource_size & 1:
            block_end += 1
        if block_end > end:
            break

        if resource_id == 0x040F:
            return data[block_start:block_end]
        cursor = block_end

    raise RuntimeError("ICC resource block not found")


def replace_image_resources(data: bytes, resources: bytes) -> bytes:
    """Replace the PSD image resources section with the given bytes."""
    length_offset, _, resources_end = locate_image_resources(data)
    out = bytearray()
    out += data[:length_offset]
    out += struct.pack(">I", len(resources))
    out += resources
    out += data[resources_end:]
    return bytes(out)


def extract_image_resources_bytes(data: bytes) -> bytes:
    """Extract raw bytes of image resources section."""
    _, resources_data_offset, resources_end = locate_image_resources(data)
    return data[resources_data_offset:resources_end]


def build_invalid_icc_resource_block() -> bytes:
    """Build a structurally valid but semantically invalid ICC resource block."""
    block = bytearray()
    block += b"8BIM"
    block += struct.pack(">H", 0x040F)
    block += b"\x00\x00"  # empty Pascal name + even padding
    block += struct.pack(">I", 4)
    block += b"\x00\x01\x02\x03"
    return bytes(block)


def write_file(path: pathlib.Path, data: bytes) -> None:
    path.write_bytes(data)
    print(path)


def write_valid_icc_variants(
    out_dir: pathlib.Path,
    *,
    source_names: list[str],
    valid_resource: bytes,
) -> None:
    for source_name in source_names:
        source_path = out_dir / source_name
        if not source_path.exists():
            raise RuntimeError(f"source fixture not found: {source_name}")
        write_file(
            out_dir / source_name.replace(".psd", "_valid_icc_profile.psd"),
            replace_image_resources(source_path.read_bytes(), valid_resource),
        )


def write_bad_icc_variants(
    out_dir: pathlib.Path,
    *,
    source_names: list[str],
    bad_resource: bytes,
) -> None:
    for source_name in source_names:
        source_path = out_dir / source_name
        if not source_path.exists():
            raise RuntimeError(f"source fixture not found: {source_name}")
        write_file(
            out_dir / source_name.replace(".psd", "_bad_icc_profile.psd"),
            replace_image_resources(source_path.read_bytes(), bad_resource),
        )


def write_malformed_icc_variants(
    out_dir: pathlib.Path,
    *,
    source_names: list[str],
    malformed_resources: bytes,
) -> None:
    for source_name in source_names:
        source_path = out_dir / source_name
        if not source_path.exists():
            raise RuntimeError(f"source fixture not found: {source_name}")
        write_file(
            out_dir / source_name.replace(".psd", "_malformed_resource.psd"),
            replace_image_resources(source_path.read_bytes(), malformed_resources),
        )


def generate(out_dir: pathlib.Path) -> None:
    base_path = out_dir / "stbi_minimal.psd"
    base = base_path.read_bytes()
    layer_len_off, compression_off, image_data_off = locate_offsets(base)

    # PSB minimal fixtures: version=2 and 64-bit layer/mask section length field.
    version2 = convert_psd_minimal_to_psb(base, convert_rle_row_table=False)
    write_file(out_dir / "stbi_minimal_version2_rgb.psd", version2)
    version2_rle = convert_psd_minimal_to_psb(
        (out_dir / "stbi_minimal_rle.psd").read_bytes(),
        convert_rle_row_table=True,
    )
    write_file(out_dir / "stbi_minimal_version2_rgb_rle.psd", version2_rle)

    # Unsupported: compression id must be 0..3.
    compression4 = bytearray(base)
    write_u16be(compression4, compression_off, 4)
    write_file(out_dir / "stbi_minimal_compression4_rgb.psd", bytes(compression4))

    # Unsupported: RGB depth must be 8/16/32 in this implementation.
    rgb1 = bytearray(base)
    write_u16be(rgb1, 22, 1)  # depth
    write_u16be(rgb1, 24, 3)  # RGB mode
    write_file(out_dir / "stbi_minimal_rgb1bit_raw.psd", bytes(rgb1))

    # Unsupported: Bitmap mode must use depth=1.
    bitmap8 = bytearray(base)
    write_u16be(bitmap8, 22, 8)  # depth
    write_u16be(bitmap8, 24, 0)  # Bitmap mode
    write_file(out_dir / "stbi_minimal_bitmap8_raw.psd", bytes(bitmap8))

    # Unsupported: Grayscale/Duotone mode depth=1 exercises %s trace path.
    gray1 = bytearray(base)
    write_u16be(gray1, 22, 1)  # depth
    write_u16be(gray1, 24, 1)  # Grayscale mode
    write_file(out_dir / "stbi_minimal_gray1bit_raw.psd", bytes(gray1))

    duotone1 = bytearray(base)
    write_u16be(duotone1, 22, 1)  # depth
    write_u16be(duotone1, 24, 8)  # Duotone mode
    write_file(out_dir / "stbi_minimal_duotone1bit_raw.psd", bytes(duotone1))

    # Unsupported: Indexed depth must be 8 in PSD.
    indexed16 = bytearray(base)
    write_u16be(indexed16, 22, 16)  # depth
    write_u16be(indexed16, 24, 2)  # Indexed mode
    write_file(out_dir / "stbi_minimal_indexed16_raw.psd", bytes(indexed16))

    # Unsupported: unimplemented color mode path (other than mode 7).
    mode6 = bytearray(base)
    write_u16be(mode6, 24, 6)
    write_file(out_dir / "stbi_minimal_colormode6_raw.psd", bytes(mode6))

    # Unsupported: Multichannel (mode=7) policy allows only channels=3/4
    # and depth=8/16/32.
    mode7_channels5 = bytearray(base)
    write_u16be(mode7_channels5, 12, 5)
    write_u16be(mode7_channels5, 24, 7)
    write_file(
        out_dir / "stbi_minimal_mode7_channels5_raw.psd",
        bytes(mode7_channels5),
    )

    mode7_channels56 = bytearray(base)
    write_u16be(mode7_channels56, 12, 56)
    write_u16be(mode7_channels56, 24, 7)
    write_file(
        out_dir / "stbi_minimal_mode7_channels56_raw.psd",
        bytes(mode7_channels56),
    )

    mode7_depth1_channels3 = bytearray(base)
    write_u16be(mode7_depth1_channels3, 12, 3)
    write_u16be(mode7_depth1_channels3, 22, 1)
    write_u16be(mode7_depth1_channels3, 24, 7)
    write_file(
        out_dir / "stbi_minimal_mode7_depth1_channels3_raw.psd",
        bytes(mode7_depth1_channels3),
    )

    mode7_depth1_channels4 = bytearray(base)
    write_u16be(mode7_depth1_channels4, 12, 4)
    write_u16be(mode7_depth1_channels4, 22, 1)
    write_u16be(mode7_depth1_channels4, 24, 7)
    write_file(
        out_dir / "stbi_minimal_mode7_depth1_channels4_raw.psd",
        bytes(mode7_depth1_channels4),
    )

    # Mode7 CMYK-mapped bad-ICC fixtures (channels=4 -> CMYK decode path).
    mode7_cmyk_zip_bad_icc = bytearray(
        (out_dir / "stbi_minimal_cmyk8_zip_bad_icc.psd").read_bytes()
    )
    write_u16be(mode7_cmyk_zip_bad_icc, 24, 7)
    write_file(
        out_dir / "stbi_minimal_mode7_cmyk8_zip_bad_icc.psd",
        bytes(mode7_cmyk_zip_bad_icc),
    )

    mode7_cmyk_zip_pred_bad_icc = bytearray(
        (out_dir / "stbi_minimal_cmyk8_zip_pred_bad_icc.psd").read_bytes()
    )
    write_u16be(mode7_cmyk_zip_pred_bad_icc, 24, 7)
    write_file(
        out_dir / "stbi_minimal_mode7_cmyk8_zip_pred_bad_icc.psd",
        bytes(mode7_cmyk_zip_pred_bad_icc),
    )

    mode7_cmyk_raw_bad_icc = bytearray(
        (out_dir / "stbi_minimal_cmyk8_bad_icc_profile.psd").read_bytes()
    )
    write_u16be(mode7_cmyk_raw_bad_icc, 24, 7)
    write_file(
        out_dir / "stbi_minimal_mode7_cmyk8_bad_icc_profile.psd",
        bytes(mode7_cmyk_raw_bad_icc),
    )

    # Mode7 CMYK-mapped valid-ICC fixtures for higher bit depths.
    # Reuse the known-good ICC resource block from the static 8-bit fixture.
    mode7_valid_icc_resource = extract_icc_resource_block(
        (out_dir / "stbi_minimal_mode7_cmyk8_valid_icc_profile.psd").read_bytes()
    )

    mode7_cmyk8_valid_icc = bytearray((out_dir / "stbi_minimal_cmyk8.psd").read_bytes())
    write_u16be(mode7_cmyk8_valid_icc, 24, 7)
    write_file(
        out_dir / "stbi_minimal_mode7_cmyk8_valid_icc_profile.psd",
        replace_image_resources(bytes(mode7_cmyk8_valid_icc), mode7_valid_icc_resource),
    )

    mode7_cmyk16_valid_icc = bytearray((out_dir / "stbi_minimal_cmyk16_raw.psd").read_bytes())
    write_u16be(mode7_cmyk16_valid_icc, 24, 7)
    write_file(
        out_dir / "stbi_minimal_mode7_cmyk16_valid_icc_profile.psd",
        replace_image_resources(bytes(mode7_cmyk16_valid_icc), mode7_valid_icc_resource),
    )

    mode7_cmyk32_valid_icc = bytearray((out_dir / "stbi_minimal_cmyk32_raw.psd").read_bytes())
    write_u16be(mode7_cmyk32_valid_icc, 24, 7)
    write_file(
        out_dir / "stbi_minimal_mode7_cmyk32_valid_icc_profile.psd",
        replace_image_resources(bytes(mode7_cmyk32_valid_icc), mode7_valid_icc_resource),
    )

    mode7_bad_resource_signature = bytearray(
        (out_dir / "stbi_minimal_bad_resource_signature.psd").read_bytes()
    )
    write_u16be(mode7_bad_resource_signature, 24, 7)
    write_file(
        out_dir / "stbi_minimal_mode7_bad_resource_signature.psd",
        bytes(mode7_bad_resource_signature),
    )

    # Malformed: channel count below mode-specific minima.
    rgb_channels2 = bytearray(base)
    write_u16be(rgb_channels2, 12, 2)
    write_u16be(rgb_channels2, 24, 3)  # RGB mode (requires >=3)
    write_file(out_dir / "stbi_minimal_rgb_channels2_raw.psd", bytes(rgb_channels2))

    cmyk_channels3 = bytearray(base)
    write_u16be(cmyk_channels3, 12, 3)
    write_u16be(cmyk_channels3, 24, 4)  # CMYK mode (requires >=4)
    write_file(
        out_dir / "stbi_minimal_cmyk_channels3_raw.psd",
        bytes(cmyk_channels3),
    )

    lab_channels2 = bytearray(base)
    write_u16be(lab_channels2, 12, 2)
    write_u16be(lab_channels2, 24, 9)  # Lab mode (requires >=3)
    write_file(out_dir / "stbi_minimal_lab_channels2_raw.psd", bytes(lab_channels2))

    # Malformed: no image data and no layer records.
    no_image = bytearray(base)
    write_u16be(no_image, compression_off, 0)
    write_file(
        out_dir / "stbi_minimal_missing_image_data_nolayer.psd",
        bytes(no_image[:image_data_off]),
    )

    # Malformed: no image data and malformed layer/mask section.
    # layer_mask_length=4, layer_info_length=1 is structurally inconsistent.
    bad_layer = bytearray()
    bad_layer += base[:layer_len_off]
    bad_layer += struct.pack(">I", 4)
    bad_layer += struct.pack(">I", 1)
    bad_layer += struct.pack(">H", 0)  # compression marker
    write_file(
        out_dir / "stbi_minimal_missing_image_data_badlayer.psd",
        bytes(bad_layer),
    )

    # Unsupported: missing composite image for non-RGB8 mode.
    missing_composite_base = (
        out_dir / "stbi_minimal_missing_composite_rgb.psd"
    ).read_bytes()
    missing_composite_gray = bytearray(missing_composite_base)
    write_u16be(missing_composite_gray, 24, 1)
    write_file(
        out_dir / "stbi_minimal_missing_composite_gray.psd",
        bytes(missing_composite_gray),
    )

    # Layer-fallback policy fixtures (based on the single-layer RGB8 baseline).
    layer_base_path = out_dir / "snake16_rgb8_missing_composite_single_layer.psd"
    layer_base = layer_base_path.read_bytes()
    layer_len_off2, _, _ = locate_offsets(layer_base)
    section_offset = layer_len_off2 + 4
    layer_info_length = read_u32be(layer_base, section_offset)
    layer_info_offset = section_offset + 4
    layer_info_end = layer_info_offset + layer_info_length

    layer_record_offset = layer_info_offset + 2
    channel_count_offset = layer_record_offset + 16
    channel_count = read_u16be(layer_base, channel_count_offset)
    channel_entries_offset = channel_count_offset + 2
    blend_block_offset = channel_entries_offset + channel_count * 6
    extra_data_length_offset = blend_block_offset + 12
    extra_data_length = read_u32be(layer_base, extra_data_length_offset)
    channel_data_offset = blend_block_offset + 16 + extra_data_length
    first_channel_length_offset = channel_entries_offset + 2
    first_channel_payload_offset = channel_data_offset

    if layer_info_end > len(layer_base):
        raise RuntimeError("invalid single-layer baseline fixture")

    fallback_channels2 = bytearray(layer_base)
    write_u16be(fallback_channels2, channel_count_offset, 2)
    write_file(
        out_dir / "snake16_rgb8_missing_composite_single_layer_channel_count2.psd",
        bytes(fallback_channels2),
    )

    fallback_table_overflow = bytearray(layer_base)
    write_u16be(fallback_table_overflow, channel_count_offset, 56)
    write_u32be(fallback_table_overflow, section_offset, 300)
    write_file(
        out_dir
        / "snake16_rgb8_missing_composite_single_layer_channel_table_overflow.psd",
        bytes(fallback_table_overflow),
    )

    fallback_length1 = bytearray(layer_base)
    write_u32be(fallback_length1, first_channel_length_offset, 1)
    write_file(
        out_dir / "snake16_rgb8_missing_composite_single_layer_channel_length1.psd",
        bytes(fallback_length1),
    )

    fallback_compression2 = bytearray(layer_base)
    write_u16be(fallback_compression2, first_channel_payload_offset, 2)
    write_file(
        out_dir
        / "snake16_rgb8_missing_composite_single_layer_channel_compression2.psd",
        bytes(fallback_compression2),
    )

    fallback_extra_oversized = bytearray(layer_base)
    write_u32be(fallback_extra_oversized, extra_data_length_offset, 0x7FFFFFFF)
    write_file(
        out_dir
        / "snake16_rgb8_missing_composite_single_layer_extra_data_oversized.psd",
        bytes(fallback_extra_oversized),
    )

    fallback_blend_short = bytearray(layer_base)
    short_layer_info_length = (blend_block_offset - layer_info_offset) + 15
    write_u32be(fallback_blend_short, section_offset, short_layer_info_length)
    write_file(
        out_dir
        / "snake16_rgb8_missing_composite_single_layer_blend_block_short.psd",
        bytes(fallback_blend_short),
    )

    fallback_record_header_short = bytearray(layer_base)
    write_u32be(fallback_record_header_short, section_offset, 1)
    write_file(
        out_dir
        / "snake16_rgb8_missing_composite_single_layer_record_header_short.psd",
        bytes(fallback_record_header_short),
    )

    fallback_record_geometry_short = bytearray(layer_base)
    write_u32be(fallback_record_geometry_short, section_offset, 2)
    write_u16be(fallback_record_geometry_short, layer_info_offset, 1)
    write_file(
        out_dir
        / "snake16_rgb8_missing_composite_single_layer_record_geometry_short.psd",
        bytes(fallback_record_geometry_short),
    )

    fallback_stream_cursor_overflow = bytearray(layer_base)
    write_u32be(fallback_stream_cursor_overflow, first_channel_length_offset, 10000)
    write_file(
        out_dir
        / "snake16_rgb8_missing_composite_single_layer_channel_stream_cursor_overflow.psd",
        bytes(fallback_stream_cursor_overflow),
    )

    fallback_stream_decode_short = bytearray(layer_base)
    write_u32be(fallback_stream_decode_short, first_channel_length_offset, 10)
    write_file(
        out_dir
        / "snake16_rgb8_missing_composite_single_layer_channel_stream_decode_short.psd",
        bytes(fallback_stream_decode_short),
    )

    # Missing-composite CMYK ICC policy fixtures for mode4/mode7 paths.
    # Reuse known-good ICC image-resource block from minimal valid profile.
    mode7_valid_icc_resource = extract_icc_resource_block(
        (out_dir / "stbi_minimal_mode7_cmyk8_valid_icc_profile.psd").read_bytes()
    )
    mode7_bad_icc_resource = build_invalid_icc_resource_block()
    mode7_malformed_resources = extract_image_resources_bytes(
        (out_dir / "stbi_minimal_mode7_bad_resource_signature.psd").read_bytes()
    )
    mode4_valid_icc_resource = extract_icc_resource_block(
        (out_dir / "stbi_minimal_mode7_cmyk8_valid_icc_profile.psd").read_bytes()
    )
    mode4_bad_icc_resource = build_invalid_icc_resource_block()
    mode4_malformed_resources = extract_image_resources_bytes(
        (out_dir / "stbi_minimal_bad_resource_signature.psd").read_bytes()
    )
    tysh_valid_suffixes = [
        "nonpixel_tysh_descriptor",
        "nonpixel_nopixel_tysh_descriptor",
        "nonpixel_nopixel_tysh_descriptor_gray",
        "nonpixel_nopixel_tysh_descriptor_cmyk",
        "nonpixel_nopixel_tysh_descriptor_hsb",
        "nonpixel_nopixel_tysh_descriptor_lab",
        "nonpixel_nopixel_tysh_wrapped_descriptor",
        "nonpixel_nopixel_tysh_wrapped_unknown_descriptor",
        "nonpixel_nopixel_tysh_wrapped_malformed_descriptor",
        "nonpixel_nopixel_tysh_enginedata_fillcolor_rgb",
        "nonpixel_nopixel_tysh_enginedata_fillcolor_values_cmyk",
        "nonpixel_nopixel_tysh_enginedata_fillcolor_values_named_cmyk",
        "nonpixel_nopixel_tysh_enginedata_fillcolor_values_named_hsb",
        "nonpixel_nopixel_tysh_enginedata_fillcolor_values_gray",
        "nonpixel_nopixel_tysh_enginedata_fillcolor_stylesheet_values_cmyk",
        "nonpixel_nopixel_tysh_enginedata_fillcolor_stylesheet_values_named_hsb",
        "nonpixel_nopixel_tysh_enginedata_fillcolor_stylesheet_values_gray",
        "nonpixel_nopixel_tysh_enginedata_fillcolor_stylesheet_values_lab",
    ]
    tysh_enginedata_suffixes = [
        "nonpixel_nopixel_tysh_enginedata_fillcolor_rgb",
        "nonpixel_nopixel_tysh_enginedata_fillcolor_values_cmyk",
        "nonpixel_nopixel_tysh_enginedata_fillcolor_values_named_cmyk",
        "nonpixel_nopixel_tysh_enginedata_fillcolor_values_named_hsb",
        "nonpixel_nopixel_tysh_enginedata_fillcolor_values_gray",
        "nonpixel_nopixel_tysh_enginedata_fillcolor_stylesheet_values_cmyk",
        "nonpixel_nopixel_tysh_enginedata_fillcolor_stylesheet_values_named_hsb",
        "nonpixel_nopixel_tysh_enginedata_fillcolor_stylesheet_values_gray",
        "nonpixel_nopixel_tysh_enginedata_fillcolor_stylesheet_values_lab",
    ]
    fill_valid_suffixes = [
        "fill_soco_descriptor",
        "fill_soco_descriptor_cmyk",
        "fill_soco_descriptor_gray",
        "fill_soco_descriptor_hsb",
        "fill_soco_descriptor_lab",
        "fill_gdfl_descriptor",
        "fill_gdfl_descriptor_cmyk",
        "fill_gdfl_descriptor_gray",
        "fill_gdfl_descriptor_hsb",
        "fill_gdfl_descriptor_lab",
        "fill_ptfl_descriptor",
        "fill_ptfl_descriptor_cmyk",
        "fill_ptfl_descriptor_gray",
        "fill_ptfl_descriptor_hsb",
        "fill_ptfl_descriptor_lab",
    ]
    tysh_bad_rotation = {
        "8": "nonpixel_nopixel_tysh_descriptor_gray",
        "16": "nonpixel_nopixel_tysh_descriptor_hsb",
        "32": "nonpixel_nopixel_tysh_descriptor_cmyk",
    }
    tysh_malformed_rotation = {
        "8": "nonpixel_nopixel_tysh_descriptor_cmyk",
        "16": "nonpixel_nopixel_tysh_descriptor_lab",
        "32": "nonpixel_nopixel_tysh_descriptor_gray",
    }
    fill_bad_rotation = {
        "8": "fill_soco_descriptor",
        "16": "fill_gdfl_descriptor_gray",
        "32": "fill_ptfl_descriptor_lab",
    }
    fill_malformed_rotation = {
        "8": "fill_soco_descriptor_cmyk",
        "16": "fill_gdfl_descriptor_hsb",
        "32": "fill_ptfl_descriptor",
    }

    for depth_tag in ("8", "16", "32"):
        mode7_prefix = f"snake16_mode7_cmyk{depth_tag}_missing_composite_multilayer"
        mode4_prefix = f"snake16_cmyk{depth_tag}_missing_composite_multilayer"

        mode7_valid_sources = (
            [f"{mode7_prefix}_normal.psd"]
            + [f"{mode7_prefix}_{suffix}.psd" for suffix in tysh_valid_suffixes]
            + [f"{mode7_prefix}_{suffix}.psd" for suffix in fill_valid_suffixes]
        )
        mode4_valid_sources = (
            [f"{mode4_prefix}_normal.psd"]
            + [f"{mode4_prefix}_{suffix}.psd" for suffix in tysh_valid_suffixes]
            + [f"{mode4_prefix}_{suffix}.psd" for suffix in fill_valid_suffixes]
        )
        write_valid_icc_variants(
            out_dir,
            source_names=mode7_valid_sources,
            valid_resource=mode7_valid_icc_resource,
        )
        write_valid_icc_variants(
            out_dir,
            source_names=mode4_valid_sources,
            valid_resource=mode4_valid_icc_resource,
        )

        mode7_bad_sources = [
            f"{mode7_prefix}_normal.psd",
            f"{mode7_prefix}_{tysh_bad_rotation[depth_tag]}.psd",
            f"{mode7_prefix}_{fill_bad_rotation[depth_tag]}.psd",
        ]
        mode7_malformed_sources = [
            f"{mode7_prefix}_normal.psd",
            f"{mode7_prefix}_{tysh_malformed_rotation[depth_tag]}.psd",
            f"{mode7_prefix}_{fill_malformed_rotation[depth_tag]}.psd",
        ]
        mode4_bad_sources = [
            f"{mode4_prefix}_normal.psd",
            f"{mode4_prefix}_{tysh_bad_rotation[depth_tag]}.psd",
            f"{mode4_prefix}_{fill_bad_rotation[depth_tag]}.psd",
        ]
        mode4_malformed_sources = [
            f"{mode4_prefix}_normal.psd",
            f"{mode4_prefix}_{tysh_malformed_rotation[depth_tag]}.psd",
            f"{mode4_prefix}_{fill_malformed_rotation[depth_tag]}.psd",
        ]
        if depth_tag == "8":
            for suffix in tysh_enginedata_suffixes:
                mode7_bad_sources.append(f"{mode7_prefix}_{suffix}.psd")
                mode7_malformed_sources.append(f"{mode7_prefix}_{suffix}.psd")
                mode4_bad_sources.append(f"{mode4_prefix}_{suffix}.psd")
                mode4_malformed_sources.append(f"{mode4_prefix}_{suffix}.psd")
        write_bad_icc_variants(
            out_dir,
            source_names=mode7_bad_sources,
            bad_resource=mode7_bad_icc_resource,
        )
        write_malformed_icc_variants(
            out_dir,
            source_names=mode7_malformed_sources,
            malformed_resources=mode7_malformed_resources,
        )
        write_bad_icc_variants(
            out_dir,
            source_names=mode4_bad_sources,
            bad_resource=mode4_bad_icc_resource,
        )
        write_malformed_icc_variants(
            out_dir,
            source_names=mode4_malformed_sources,
            malformed_resources=mode4_malformed_resources,
        )


def main() -> int:
    out_dir = pathlib.Path(__file__).resolve().parent
    generate(out_dir)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
