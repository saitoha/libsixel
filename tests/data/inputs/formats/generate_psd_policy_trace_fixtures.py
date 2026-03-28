#!/usr/bin/env python3
"""Generate static minimal PSD fixtures for policy/trace tests.

This script is for fixture regeneration only.
It is never invoked from TAP tests.
"""

import pathlib
import struct


def read_u32be(data: bytes, offset: int) -> int:
    return struct.unpack(">I", data[offset : offset + 4])[0]


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

    # Unsupported: Indexed depth must be 8 in PSD.
    indexed16 = bytearray(base)
    write_u16be(indexed16, 22, 16)  # depth
    write_u16be(indexed16, 24, 2)  # Indexed mode
    write_file(out_dir / "stbi_minimal_indexed16_raw.psd", bytes(indexed16))

    # Unsupported: unimplemented color mode path (other than mode 7).
    mode6 = bytearray(base)
    write_u16be(mode6, 24, 6)
    write_file(out_dir / "stbi_minimal_colormode6_raw.psd", bytes(mode6))

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


def main() -> int:
    out_dir = pathlib.Path(__file__).resolve().parent
    generate(out_dir)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
