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


def write_file(path: pathlib.Path, data: bytes) -> None:
    path.write_bytes(data)
    print(path)


def generate(out_dir: pathlib.Path) -> None:
    base_path = out_dir / "stbi_minimal.psd"
    base = base_path.read_bytes()
    layer_len_off, compression_off, image_data_off = locate_offsets(base)

    # Unsupported: header version must be 1.
    version2 = bytearray(base)
    write_u16be(version2, 4, 2)
    write_file(out_dir / "stbi_minimal_version2_rgb.psd", bytes(version2))

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

    mode7_depth1_channels3 = bytearray(base)
    write_u16be(mode7_depth1_channels3, 12, 3)
    write_u16be(mode7_depth1_channels3, 22, 1)
    write_u16be(mode7_depth1_channels3, 24, 7)
    write_file(
        out_dir / "stbi_minimal_mode7_depth1_channels3_raw.psd",
        bytes(mode7_depth1_channels3),
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


def main() -> int:
    out_dir = pathlib.Path(__file__).resolve().parent
    generate(out_dir)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
