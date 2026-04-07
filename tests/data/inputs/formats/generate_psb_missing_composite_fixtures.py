#!/usr/bin/env python3
"""Generate PSB (8BPB/version2) fixtures from PSD missing-composite fixtures.

This script converts layer-only PSD fixtures that use 32-bit layer lengths into
PSB layout (64-bit layer/mask length fields, 64-bit per-channel lengths).
Optionally, it can rewrite per-layer channel payloads to RLE with PSB 4-byte
row length tables.
"""

from __future__ import annotations

import argparse
import pathlib
import struct


HERE = pathlib.Path(__file__).resolve().parent


def read_u16be(data: bytes, off: int) -> int:
    return struct.unpack_from(">H", data, off)[0]


def read_i16be(data: bytes, off: int) -> int:
    return struct.unpack_from(">h", data, off)[0]


def read_u32be(data: bytes, off: int) -> int:
    return struct.unpack_from(">I", data, off)[0]


def read_u64be(data: bytes, off: int) -> int:
    return struct.unpack_from(">Q", data, off)[0]


def write_u32be(buf: bytearray, off: int, value: int) -> None:
    struct.pack_into(">I", buf, off, value & 0xFFFFFFFF)


def write_u64be(buf: bytearray, off: int, value: int) -> None:
    struct.pack_into(">Q", buf, off, value & 0xFFFFFFFFFFFFFFFF)


def packbits_encode_row(row: bytes) -> bytes:
    out = bytearray()
    i = 0
    n = len(row)
    while i < n:
        run = 1
        while i + run < n and run < 128 and row[i + run] == row[i]:
            run += 1
        if run >= 3:
            out.append((257 - run) & 0xFF)
            out.append(row[i])
            i += run
            continue

        lit_start = i
        i += 1
        while i < n:
            run = 1
            while i + run < n and run < 128 and row[i + run] == row[i]:
                run += 1
            if run >= 3:
                break
            i += 1
            if i - lit_start >= 128:
                break
        lit_len = i - lit_start
        out.append((lit_len - 1) & 0xFF)
        out += row[lit_start:i]
    return bytes(out)


def encode_rle_payload_from_raw(raw: bytes, *, row_bytes: int, height: int) -> bytes:
    if len(raw) != row_bytes * height:
        raise RuntimeError("unexpected raw plane size")
    table = []
    row_data = bytearray()
    for y in range(height):
        row = raw[y * row_bytes : (y + 1) * row_bytes]
        enc = packbits_encode_row(row)
        table.append(len(enc))
        row_data += enc
    payload = bytearray()
    payload += struct.pack(">H", 1)
    for n in table:
        payload += struct.pack(">I", n)
    payload += row_data
    return bytes(payload)


def convert_rle_row_table_to_u32(payload: bytes, *, height: int) -> bytes:
    if len(payload) < 2:
        raise RuntimeError("short channel payload")
    compression = read_u16be(payload, 0)
    if compression != 1:
        return payload
    table_off = 2
    table_bytes = height * 2
    if len(payload) < table_off + table_bytes:
        raise RuntimeError("short RLE row table")
    lengths = [read_u16be(payload, table_off + i * 2) for i in range(height)]
    row_data = payload[table_off + table_bytes :]
    if sum(lengths) != len(row_data):
        raise RuntimeError("invalid PSD RLE row table")
    out = bytearray()
    out += struct.pack(">H", 1)
    for n in lengths:
        out += struct.pack(">I", n)
    out += row_data
    return bytes(out)


def convert_layer_payload(payload: bytes,
                          *,
                          row_bytes: int,
                          height: int,
                          force_rle: bool) -> bytes:
    if len(payload) < 2:
        raise RuntimeError("short layer channel payload")
    compression = read_u16be(payload, 0)

    if force_rle:
        if compression != 0:
            raise RuntimeError("force_rle currently expects raw layer channels")
        raw = payload[2:]
        return encode_rle_payload_from_raw(raw, row_bytes=row_bytes, height=height)

    if compression == 1:
        return convert_rle_row_table_to_u32(payload, height=height)
    return payload


def convert_psd_missing_composite_to_psb(data: bytes, *, force_rle: bool) -> bytes:
    if len(data) < 26:
        raise RuntimeError("short PSD")
    if data[:4] != b"8BPS":
        raise RuntimeError("input is not PSD(8BPS)")
    if read_u16be(data, 4) != 1:
        raise RuntimeError("input is not PSD version=1")

    channels = read_u16be(data, 12)
    height = read_u32be(data, 14)
    width = read_u32be(data, 18)
    depth = read_u16be(data, 22)
    if depth == 8:
        sample_bytes = 1
    elif depth == 16:
        sample_bytes = 2
    elif depth == 32:
        sample_bytes = 4
    else:
        raise RuntimeError(f"unsupported depth: {depth}")

    off = 26
    if off + 4 > len(data):
        raise RuntimeError("short color mode section")
    color_mode_len = read_u32be(data, off)
    off += 4
    if off + color_mode_len > len(data):
        raise RuntimeError("short color mode data")
    color_mode_data = data[off : off + color_mode_len]
    off += color_mode_len

    if off + 4 > len(data):
        raise RuntimeError("short image resources section")
    image_res_len = read_u32be(data, off)
    off += 4
    if off + image_res_len > len(data):
        raise RuntimeError("short image resources")
    image_res_data = data[off : off + image_res_len]
    off += image_res_len

    if off + 4 > len(data):
        raise RuntimeError("short layer/mask section")
    layer_mask_len = read_u32be(data, off)
    off += 4
    if off + layer_mask_len > len(data):
        raise RuntimeError("short layer/mask data")
    layer_mask_data = data[off : off + layer_mask_len]
    off += layer_mask_len

    tail = data[off:]

    if len(layer_mask_data) < 4:
        raise RuntimeError("short layer info")
    layer_info_len = read_u32be(layer_mask_data, 0)
    if 4 + layer_info_len > len(layer_mask_data):
        raise RuntimeError("invalid layer info length")
    layer_info = layer_mask_data[4 : 4 + layer_info_len]
    layer_mask_tail = layer_mask_data[4 + layer_info_len :]

    if len(layer_info) < 2:
        raise RuntimeError("short layer record table")
    layer_count_raw = read_i16be(layer_info, 0)
    layer_count = abs(layer_count_raw)
    cursor = 2

    records = []
    for _ in range(layer_count):
        if cursor + 18 > len(layer_info):
            raise RuntimeError("short layer geometry")
        top, left, bottom, right = struct.unpack_from(">iiii", layer_info, cursor)
        cursor += 16
        channel_count = read_u16be(layer_info, cursor)
        cursor += 2
        if bottom < top or right < left:
            raise RuntimeError("invalid layer geometry")
        layer_w = right - left
        layer_h = bottom - top
        row_bytes = layer_w * sample_bytes

        channels_meta = []
        for _ in range(channel_count):
            if cursor + 6 > len(layer_info):
                raise RuntimeError("short channel table")
            channel_id = read_i16be(layer_info, cursor)
            channel_len = read_u32be(layer_info, cursor + 2)
            cursor += 6
            channels_meta.append({"id": channel_id, "len": channel_len})

        if cursor + 16 > len(layer_info):
            raise RuntimeError("short layer blend block")
        sig = layer_info[cursor : cursor + 4]
        blend_key = layer_info[cursor + 4 : cursor + 8]
        opacity = layer_info[cursor + 8]
        clipping = layer_info[cursor + 9]
        flags = layer_info[cursor + 10]
        filler = layer_info[cursor + 11]
        extra_len = read_u32be(layer_info, cursor + 12)
        cursor += 16
        if cursor + extra_len > len(layer_info):
            raise RuntimeError("short layer extra")
        extra = layer_info[cursor : cursor + extra_len]
        cursor += extra_len

        records.append(
            {
                "top": top,
                "left": left,
                "bottom": bottom,
                "right": right,
                "channel_count": channel_count,
                "channels": channels_meta,
                "sig": sig,
                "blend_key": blend_key,
                "opacity": opacity,
                "clipping": clipping,
                "flags": flags,
                "filler": filler,
                "extra": extra,
                "row_bytes": row_bytes,
                "layer_h": layer_h,
            }
        )

    channel_data_cursor = cursor
    new_channel_data = bytearray()
    for rec in records:
        for ch in rec["channels"]:
            ch_len = int(ch["len"])
            if channel_data_cursor + ch_len > len(layer_info):
                raise RuntimeError("short channel payload")
            payload = layer_info[channel_data_cursor : channel_data_cursor + ch_len]
            channel_data_cursor += ch_len
            new_payload = convert_layer_payload(
                payload,
                row_bytes=int(rec["row_bytes"]),
                height=int(rec["layer_h"]),
                force_rle=force_rle,
            )
            ch["new_len"] = len(new_payload)
            new_channel_data += new_payload

    layer_info_tail = layer_info[channel_data_cursor:]

    new_layer_records = bytearray()
    for rec in records:
        new_layer_records += struct.pack(">iiii", rec["top"], rec["left"], rec["bottom"], rec["right"])
        new_layer_records += struct.pack(">H", rec["channel_count"])
        for ch in rec["channels"]:
            new_layer_records += struct.pack(">hQ", ch["id"], ch["new_len"])
        new_layer_records += rec["sig"]
        new_layer_records += rec["blend_key"]
        new_layer_records += bytes([rec["opacity"], rec["clipping"], rec["flags"], rec["filler"]])
        new_layer_records += struct.pack(">I", len(rec["extra"]))
        new_layer_records += rec["extra"]

    new_layer_info = bytearray()
    new_layer_info += struct.pack(">h", layer_count_raw)
    new_layer_info += new_layer_records
    new_layer_info += new_channel_data
    new_layer_info += layer_info_tail

    new_layer_mask_data = bytearray()
    new_layer_mask_data += struct.pack(">Q", len(new_layer_info))
    new_layer_mask_data += new_layer_info
    new_layer_mask_data += layer_mask_tail

    out = bytearray()
    out += b"8BPB"
    out += struct.pack(">H", 2)
    out += data[6:12]
    out += struct.pack(">H", channels)
    out += struct.pack(">I", height)
    out += struct.pack(">I", width)
    out += struct.pack(">H", depth)
    out += data[24:26]
    out += struct.pack(">I", len(color_mode_data))
    out += color_mode_data
    out += struct.pack(">I", len(image_res_data))
    out += image_res_data
    out += struct.pack(">Q", len(new_layer_mask_data))
    out += new_layer_mask_data
    out += tail
    return bytes(out)


def convert_fixture(src_name: str, dst_name: str, *, force_rle: bool = False) -> None:
    src = HERE / src_name
    dst = HERE / dst_name
    data = src.read_bytes()
    out = convert_psd_missing_composite_to_psb(data, force_rle=force_rle)
    dst.write_bytes(out)
    print(dst)


def write_truncated_fixture(src_name: str, dst_name: str, *, drop_tail_bytes: int) -> None:
    src = HERE / src_name
    dst = HERE / dst_name
    data = src.read_bytes()
    if drop_tail_bytes <= 0 or drop_tail_bytes >= len(data):
        raise RuntimeError("invalid truncation size")
    dst.write_bytes(data[:-drop_tail_bytes])
    print(dst)


def locate_psb_sections(data: bytes) -> dict[str, int]:
    if len(data) < 26 or data[:4] != b"8BPB" or read_u16be(data, 4) != 2:
        raise RuntimeError("fixture is not PSB version=2")

    off = 26
    color_mode_len = read_u32be(data, off)
    off += 4 + color_mode_len
    image_resources_len = read_u32be(data, off)
    off += 4 + image_resources_len

    layer_mask_length_off = off
    layer_mask_length = read_u64be(data, layer_mask_length_off)
    off += 8
    layer_mask_offset = off
    if layer_mask_offset + layer_mask_length > len(data):
        raise RuntimeError("invalid PSB layer/mask length")
    return {
        "layer_mask_length_off": layer_mask_length_off,
        "layer_mask_length": layer_mask_length,
        "layer_mask_offset": layer_mask_offset,
        "layer_info_length_off": layer_mask_offset,
    }


def locate_psb_top_sections(data: bytes) -> dict[str, int]:
    if len(data) < 26 or data[:4] != b"8BPB" or read_u16be(data, 4) != 2:
        raise RuntimeError("fixture is not PSB version=2")

    off = 26
    color_mode_length_off = off
    color_mode_length = read_u32be(data, color_mode_length_off)
    off += 4 + color_mode_length
    image_resources_length_off = off
    image_resources_length = read_u32be(data, image_resources_length_off)
    off += 4
    image_resources_data_off = off
    layer_mask_length_off = image_resources_data_off + image_resources_length
    if layer_mask_length_off + 8 > len(data):
        raise RuntimeError("short PSB layer/mask header")

    layer_mask_length = read_u64be(data, layer_mask_length_off)
    layer_mask_offset = layer_mask_length_off + 8
    if layer_mask_offset + layer_mask_length > len(data):
        raise RuntimeError("invalid PSB layer/mask length")
    return {
        "color_mode_length_off": color_mode_length_off,
        "color_mode_length": color_mode_length,
        "image_resources_length_off": image_resources_length_off,
        "image_resources_length": image_resources_length,
        "image_resources_data_off": image_resources_data_off,
        "layer_mask_length_off": layer_mask_length_off,
        "layer_mask_length": layer_mask_length,
        "layer_mask_offset": layer_mask_offset,
    }


def parse_psb_first_channel(data: bytes) -> dict[str, int]:
    sections = locate_psb_sections(data)
    layer_mask_offset = sections["layer_mask_offset"]
    layer_mask_length = sections["layer_mask_length"]
    layer_info_length = read_u64be(data, layer_mask_offset)
    layer_info_offset = layer_mask_offset + 8
    layer_info_end = layer_info_offset + layer_info_length
    layer_mask_end = layer_mask_offset + layer_mask_length
    if layer_info_length < 2 or layer_info_end > layer_mask_end:
        raise RuntimeError("invalid PSB layer info length")

    cursor = layer_info_offset
    layer_count_raw = read_i16be(data, cursor)
    layer_count = abs(layer_count_raw)
    cursor += 2
    if layer_count <= 0:
        raise RuntimeError("invalid layer count")

    channel_entries: list[dict[str, int]] = []
    first_channel_length_off = -1

    for layer_index in range(layer_count):
        if cursor + 18 > layer_info_end:
            raise RuntimeError("short layer geometry")
        cursor += 16
        channel_count = read_u16be(data, cursor)
        cursor += 2
        if channel_count <= 0:
            raise RuntimeError("invalid channel count")
        for channel_index in range(channel_count):
            if cursor + 10 > layer_info_end:
                raise RuntimeError("short layer channel table")
            length_off = cursor + 2
            channel_length = read_u64be(data, length_off)
            if layer_index == 0 and channel_index == 0:
                first_channel_length_off = length_off
            channel_entries.append(
                {
                    "length_off": length_off,
                    "length": channel_length,
                }
            )
            cursor += 10
        if cursor + 16 > layer_info_end:
            raise RuntimeError("short layer blend block")
        extra_len = read_u32be(data, cursor + 12)
        cursor += 16
        if cursor + extra_len > layer_info_end:
            raise RuntimeError("short layer extra data")
        cursor += extra_len

    channel_data_cursor = cursor
    first_channel_data_offset = -1
    for i, entry in enumerate(channel_entries):
        channel_length = entry["length"]
        if channel_data_cursor + channel_length > layer_info_end:
            raise RuntimeError("short layer channel stream")
        if i == 0:
            first_channel_data_offset = channel_data_cursor
        channel_data_cursor += channel_length

    if first_channel_length_off < 0 or first_channel_data_offset < 0:
        raise RuntimeError("failed to locate first channel")
    first_channel_max_length = layer_info_end - first_channel_data_offset
    return {
        "first_channel_length_off": first_channel_length_off,
        "first_channel_length": read_u64be(data, first_channel_length_off),
        "first_channel_data_offset": first_channel_data_offset,
        "first_channel_max_length": first_channel_max_length,
        "layer_info_end": layer_info_end,
    }


def parse_psb_first_layer_geometry(data: bytes) -> dict[str, int]:
    sections = locate_psb_sections(data)
    layer_mask_offset = sections["layer_mask_offset"]
    layer_mask_length = sections["layer_mask_length"]
    layer_info_length = read_u64be(data, layer_mask_offset)
    layer_info_offset = layer_mask_offset + 8
    layer_info_end = layer_info_offset + layer_info_length
    if layer_info_length < 2 or layer_info_end > layer_mask_offset + layer_mask_length:
        raise RuntimeError("invalid PSB layer info length")
    cursor = layer_info_offset
    layer_count = abs(read_i16be(data, cursor))
    cursor += 2
    if layer_count <= 0 or cursor + 18 > layer_info_end:
        raise RuntimeError("missing first layer geometry")
    return {
        "top_off": cursor + 0,
        "left_off": cursor + 4,
        "bottom_off": cursor + 8,
        "right_off": cursor + 12,
        "top": struct.unpack_from(">i", data, cursor + 0)[0],
        "left": struct.unpack_from(">i", data, cursor + 4)[0],
        "bottom": struct.unpack_from(">i", data, cursor + 8)[0],
        "right": struct.unpack_from(">i", data, cursor + 12)[0],
    }


def mutate_psb_layer_mask_length_overflow(src_name: str, dst_name: str) -> None:
    src = HERE / src_name
    dst = HERE / dst_name
    data = bytearray(src.read_bytes())
    sections = locate_psb_sections(data)
    overflow_length = len(data) - sections["layer_mask_offset"] + 1
    write_u64be(data, sections["layer_mask_length_off"], overflow_length)
    dst.write_bytes(bytes(data))
    print(dst)


def mutate_psb_layer_info_length_overflow(src_name: str, dst_name: str) -> None:
    src = HERE / src_name
    dst = HERE / dst_name
    data = bytearray(src.read_bytes())
    sections = locate_psb_sections(data)
    overflow_length = sections["layer_mask_length"] + 1
    write_u64be(data, sections["layer_info_length_off"], overflow_length)
    dst.write_bytes(bytes(data))
    print(dst)


def mutate_psb_channel_length_overflow(src_name: str, dst_name: str) -> None:
    src = HERE / src_name
    dst = HERE / dst_name
    data = bytearray(src.read_bytes())
    channel = parse_psb_first_channel(data)
    overflow_length = len(data)
    write_u64be(data, channel["first_channel_length_off"], overflow_length)
    dst.write_bytes(bytes(data))
    print(dst)


def mutate_psb_layer_mask_length_u64max(src_name: str, dst_name: str) -> None:
    src = HERE / src_name
    dst = HERE / dst_name
    data = bytearray(src.read_bytes())
    sections = locate_psb_sections(data)
    write_u64be(data, sections["layer_mask_length_off"], 0xFFFFFFFFFFFFFFFF)
    dst.write_bytes(bytes(data))
    print(dst)


def mutate_psb_layer_info_length_u64max(src_name: str, dst_name: str) -> None:
    src = HERE / src_name
    dst = HERE / dst_name
    data = bytearray(src.read_bytes())
    sections = locate_psb_sections(data)
    write_u64be(data, sections["layer_info_length_off"], 0xFFFFFFFFFFFFFFFF)
    dst.write_bytes(bytes(data))
    print(dst)


def mutate_psb_channel_length_u64max(src_name: str, dst_name: str) -> None:
    src = HERE / src_name
    dst = HERE / dst_name
    data = bytearray(src.read_bytes())
    channel = parse_psb_first_channel(data)
    write_u64be(data, channel["first_channel_length_off"], 0xFFFFFFFFFFFFFFFF)
    dst.write_bytes(bytes(data))
    print(dst)


def mutate_psb_rle_row_table_too_short(src_name: str, dst_name: str) -> None:
    src = HERE / src_name
    dst = HERE / dst_name
    data = bytearray(src.read_bytes())
    channel = parse_psb_first_channel(data)
    geom = parse_psb_first_layer_geometry(data)
    if read_u16be(data, channel["first_channel_data_offset"]) != 1:
        raise RuntimeError("first channel is not RLE payload")
    # Force a much larger layer height while preserving payload.
    # This keeps the layer stream structurally valid and makes the PSB
    # 4-byte row-table requirement exceed available payload bytes.
    new_bottom = geom["top"] + 1000
    write_u32be(data, geom["bottom_off"], new_bottom)
    dst.write_bytes(bytes(data))
    print(dst)


def mutate_psb_rle_row_length_overrun(src_name: str, dst_name: str) -> None:
    src = HERE / src_name
    dst = HERE / dst_name
    data = bytearray(src.read_bytes())
    channel = parse_psb_first_channel(data)
    if read_u16be(data, channel["first_channel_data_offset"]) != 1:
        raise RuntimeError("first channel is not RLE payload")
    first_row_length_off = channel["first_channel_data_offset"] + 2
    write_u32be(data, first_row_length_off, 0x7FFFFFFF)
    dst.write_bytes(bytes(data))
    print(dst)


def mutate_psb_layer_info_end_overrun(src_name: str, dst_name: str) -> None:
    src = HERE / src_name
    dst = HERE / dst_name
    data = bytearray(src.read_bytes())
    sections = locate_psb_sections(data)
    write_u64be(
        data,
        sections["layer_info_length_off"],
        sections["layer_mask_length"] + 1,
    )
    dst.write_bytes(bytes(data))
    print(dst)


def mutate_psb_channel_window_overrun(src_name: str, dst_name: str) -> None:
    src = HERE / src_name
    dst = HERE / dst_name
    data = bytearray(src.read_bytes())
    channel = parse_psb_first_channel(data)
    write_u64be(
        data,
        channel["first_channel_length_off"],
        channel["first_channel_max_length"] + 1,
    )
    dst.write_bytes(bytes(data))
    print(dst)


def mutate_psb_rle_payload_window_overrun(src_name: str, dst_name: str) -> None:
    src = HERE / src_name
    dst = HERE / dst_name
    data = bytearray(src.read_bytes())
    channel = parse_psb_first_channel(data)
    geom = parse_psb_first_layer_geometry(data)
    if read_u16be(data, channel["first_channel_data_offset"]) != 1:
        raise RuntimeError("first channel is not RLE payload")
    height = geom["bottom"] - geom["top"]
    if height <= 0:
        raise RuntimeError("invalid layer height")
    row_table_bytes = height * 4
    if channel["first_channel_length"] <= 2 + row_table_bytes:
        raise RuntimeError("RLE payload does not have row data")
    available_row_payload = channel["first_channel_length"] - 2 - row_table_bytes
    if available_row_payload >= 0xFFFFFFFF:
        raise RuntimeError("row payload too large for overflow fixture")
    first_row_length_off = channel["first_channel_data_offset"] + 2
    write_u32be(data, first_row_length_off, available_row_payload + 1)
    dst.write_bytes(bytes(data))
    print(dst)


def mutate_psb_high_offset_valid(src_name: str,
                                 dst_name: str,
                                 *,
                                 padding_bytes: int = 0x20000) -> None:
    src = HERE / src_name
    dst = HERE / dst_name
    data = src.read_bytes()
    sections = locate_psb_top_sections(data)
    if padding_bytes <= 0:
        raise RuntimeError("padding_bytes must be positive")
    image_resources_length = sections["image_resources_length"]
    if image_resources_length > 0xFFFFFFFF - padding_bytes:
        raise RuntimeError("image resources length overflow")

    insert_off = sections["layer_mask_length_off"]
    out = bytearray()
    out += data[:insert_off]
    out += bytes(padding_bytes)
    out += data[insert_off:]
    write_u32be(
        out,
        sections["image_resources_length_off"],
        image_resources_length + padding_bytes,
    )
    dst.write_bytes(bytes(out))
    print(dst)


def generate_high_offset_over1m_fixtures() -> None:
    scales = (
        # Keep one representative >1MiB tier to avoid combinatorial explosion.
        ("xxlarge", 0x400000),
    )

    for mode_prefix in ("cmyk", "mode7_cmyk"):
        for depth_tag in ("16", "32"):
            base = f"snake16_psb_{mode_prefix}{depth_tag}_missing_composite_multilayer"
            normal_src = f"{base}_normal.psd"
            normal_rle_src = f"{base}_normal_rle.psd"

            for scale_name, padding in scales:
                normal_valid = f"{base}_normal_high_offset_{scale_name}.psd"
                normal_rle_valid = f"{base}_normal_rle_high_offset_{scale_name}.psd"
                mutate_psb_high_offset_valid(
                    normal_src,
                    normal_valid,
                    padding_bytes=padding,
                )
                mutate_psb_high_offset_valid(
                    normal_rle_src,
                    normal_rle_valid,
                    padding_bytes=padding,
                )
                mutate_psb_layer_info_end_overrun(
                    normal_valid,
                    f"{base}_normal_high_offset_{scale_name}_layer_info_end_overrun.psd",
                )
                mutate_psb_channel_window_overrun(
                    normal_valid,
                    f"{base}_normal_high_offset_{scale_name}_channel_window_overrun.psd",
                )
                mutate_psb_rle_payload_window_overrun(
                    normal_rle_valid,
                    f"{base}_normal_rle_high_offset_{scale_name}_rle_payload_window_overrun.psd",
                )


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description="Generate PSB fixtures from PSD missing-composite fixtures.",
    )
    parser.add_argument(
        "--high-offset-over1m-only",
        action="store_true",
        help=(
            "Generate only >1MiB high-offset PSB fixtures "
            "(xxlarge and derived malformed variants)."
        ),
    )
    parser.add_argument(
        "--high-offset-large-only",
        action="store_true",
        help=argparse.SUPPRESS,
    )
    return parser.parse_args()


def main() -> None:
    args = parse_args()
    if args.high_offset_over1m_only or args.high_offset_large_only:
        generate_high_offset_over1m_fixtures()
        return

    # Representative PSB(v2) missing-composite fixtures used by loader tests.
    convert_fixture(
        "snake16_rgb8_missing_composite_multilayer_normal.psd",
        "snake16_psb_rgb8_missing_composite_multilayer_normal.psd",
    )
    convert_fixture(
        "snake16_rgb16_missing_composite_multilayer_normal.psd",
        "snake16_psb_rgb16_missing_composite_multilayer_normal.psd",
    )
    convert_fixture(
        "snake16_rgb32_missing_composite_multilayer_normal.psd",
        "snake16_psb_rgb32_missing_composite_multilayer_normal.psd",
    )
    convert_fixture(
        "snake16_cmyk8_missing_composite_multilayer_normal.psd",
        "snake16_psb_cmyk8_missing_composite_multilayer_normal.psd",
    )
    convert_fixture(
        "snake16_cmyk16_missing_composite_multilayer_normal.psd",
        "snake16_psb_cmyk16_missing_composite_multilayer_normal.psd",
    )
    convert_fixture(
        "snake16_cmyk32_missing_composite_multilayer_normal.psd",
        "snake16_psb_cmyk32_missing_composite_multilayer_normal.psd",
    )
    convert_fixture(
        "snake16_mode7_rgb16_missing_composite_multilayer_normal.psd",
        "snake16_psb_mode7_rgb16_missing_composite_multilayer_normal.psd",
    )
    convert_fixture(
        "snake16_mode7_cmyk16_missing_composite_multilayer_normal.psd",
        "snake16_psb_mode7_cmyk16_missing_composite_multilayer_normal.psd",
    )
    convert_fixture(
        "snake16_mode7_cmyk32_missing_composite_multilayer_normal.psd",
        "snake16_psb_mode7_cmyk32_missing_composite_multilayer_normal.psd",
    )
    convert_fixture(
        "snake16_rgb16_missing_composite_multilayer_clipping.psd",
        "snake16_psb_rgb16_missing_composite_multilayer_clipping.psd",
    )
    convert_fixture(
        "snake16_rgb16_missing_composite_multilayer_mask.psd",
        "snake16_psb_rgb16_missing_composite_multilayer_mask.psd",
    )
    convert_fixture(
        "snake16_cmyk16_missing_composite_multilayer_clipping.psd",
        "snake16_psb_cmyk16_missing_composite_multilayer_clipping.psd",
    )
    convert_fixture(
        "snake16_cmyk16_missing_composite_multilayer_mask.psd",
        "snake16_psb_cmyk16_missing_composite_multilayer_mask.psd",
    )
    convert_fixture(
        "snake16_mode7_cmyk32_missing_composite_multilayer_clipping.psd",
        "snake16_psb_mode7_cmyk32_missing_composite_multilayer_clipping.psd",
    )
    convert_fixture(
        "snake16_mode7_cmyk32_missing_composite_multilayer_mask.psd",
        "snake16_psb_mode7_cmyk32_missing_composite_multilayer_mask.psd",
    )
    convert_fixture(
        "snake16_mode7_rgb8_missing_composite_multilayer_nonpixel_nopixel_tysh_descriptor_gray.psd",
        "snake16_psb_mode7_rgb8_missing_composite_multilayer_nonpixel_nopixel_tysh_descriptor_gray.psd",
    )
    convert_fixture(
        "snake16_mode7_rgb8_missing_composite_multilayer_nonpixel_nopixel_tysh_descriptor_cmyk.psd",
        "snake16_psb_mode7_rgb8_missing_composite_multilayer_nonpixel_nopixel_tysh_descriptor_cmyk.psd",
    )
    convert_fixture(
        "snake16_mode7_rgb8_missing_composite_multilayer_nonpixel_nopixel_tysh_descriptor_hsb.psd",
        "snake16_psb_mode7_rgb8_missing_composite_multilayer_nonpixel_nopixel_tysh_descriptor_hsb.psd",
    )
    convert_fixture(
        "snake16_mode7_rgb8_missing_composite_multilayer_nonpixel_nopixel_tysh_descriptor_lab.psd",
        "snake16_psb_mode7_rgb8_missing_composite_multilayer_nonpixel_nopixel_tysh_descriptor_lab.psd",
    )
    convert_fixture(
        "snake16_mode7_cmyk8_missing_composite_multilayer_nonpixel_nopixel_tysh_descriptor_gray.psd",
        "snake16_psb_mode7_cmyk8_missing_composite_multilayer_nonpixel_nopixel_tysh_descriptor_gray.psd",
    )
    convert_fixture(
        "snake16_mode7_cmyk8_missing_composite_multilayer_nonpixel_nopixel_tysh_descriptor_cmyk.psd",
        "snake16_psb_mode7_cmyk8_missing_composite_multilayer_nonpixel_nopixel_tysh_descriptor_cmyk.psd",
    )
    convert_fixture(
        "snake16_mode7_cmyk8_missing_composite_multilayer_nonpixel_nopixel_tysh_descriptor_hsb.psd",
        "snake16_psb_mode7_cmyk8_missing_composite_multilayer_nonpixel_nopixel_tysh_descriptor_hsb.psd",
    )
    convert_fixture(
        "snake16_mode7_cmyk8_missing_composite_multilayer_nonpixel_nopixel_tysh_descriptor_lab.psd",
        "snake16_psb_mode7_cmyk8_missing_composite_multilayer_nonpixel_nopixel_tysh_descriptor_lab.psd",
    )
    convert_fixture(
        "snake16_cmyk8_missing_composite_multilayer_nonpixel_nopixel_tysh_enginedata_fillcolor_rgb.psd",
        "snake16_psb_cmyk8_missing_composite_multilayer_nonpixel_nopixel_tysh_enginedata_fillcolor_rgb.psd",
    )
    convert_fixture(
        "snake16_mode7_cmyk8_missing_composite_multilayer_nonpixel_nopixel_tysh_enginedata_fillcolor_rgb.psd",
        "snake16_psb_mode7_cmyk8_missing_composite_multilayer_nonpixel_nopixel_tysh_enginedata_fillcolor_rgb.psd",
    )
    convert_fixture(
        "snake16_cmyk8_missing_composite_multilayer_nonpixel_nopixel_tysh_enginedata_fillcolor_fillflag_false.psd",
        "snake16_psb_cmyk8_missing_composite_multilayer_nonpixel_nopixel_tysh_enginedata_fillcolor_fillflag_false.psd",
    )
    convert_fixture(
        "snake16_mode7_cmyk8_missing_composite_multilayer_nonpixel_nopixel_tysh_enginedata_fillcolor_fillflag_false.psd",
        "snake16_psb_mode7_cmyk8_missing_composite_multilayer_nonpixel_nopixel_tysh_enginedata_fillcolor_fillflag_false.psd",
    )
    convert_fixture(
        "snake16_cmyk8_missing_composite_multilayer_nonpixel_nopixel_tysh_enginedata_fillcolor_fillflag_true.psd",
        "snake16_psb_cmyk8_missing_composite_multilayer_nonpixel_nopixel_tysh_enginedata_fillcolor_fillflag_true.psd",
    )
    convert_fixture(
        "snake16_mode7_cmyk8_missing_composite_multilayer_nonpixel_nopixel_tysh_enginedata_fillcolor_fillflag_true.psd",
        "snake16_psb_mode7_cmyk8_missing_composite_multilayer_nonpixel_nopixel_tysh_enginedata_fillcolor_fillflag_true.psd",
    )
    convert_fixture(
        "snake16_cmyk8_missing_composite_multilayer_nonpixel_nopixel_tysh_enginedata_fillcolor_fillopacity_025.psd",
        "snake16_psb_cmyk8_missing_composite_multilayer_nonpixel_nopixel_tysh_enginedata_fillcolor_fillopacity_025.psd",
    )
    convert_fixture(
        "snake16_mode7_cmyk8_missing_composite_multilayer_nonpixel_nopixel_tysh_enginedata_fillcolor_fillopacity_050.psd",
        "snake16_psb_mode7_cmyk8_missing_composite_multilayer_nonpixel_nopixel_tysh_enginedata_fillcolor_fillopacity_050.psd",
    )
    convert_fixture(
        "snake16_cmyk8_missing_composite_multilayer_nonpixel_nopixel_tysh_enginedata_fillcolor_fillopacity_100.psd",
        "snake16_psb_cmyk8_missing_composite_multilayer_nonpixel_nopixel_tysh_enginedata_fillcolor_fillopacity_100.psd",
    )
    convert_fixture(
        "snake16_mode7_cmyk8_missing_composite_multilayer_nonpixel_nopixel_tysh_enginedata_fillcolor_fillopacity_malformed.psd",
        "snake16_psb_mode7_cmyk8_missing_composite_multilayer_nonpixel_nopixel_tysh_enginedata_fillcolor_fillopacity_malformed.psd",
    )
    convert_fixture(
        "snake16_cmyk8_missing_composite_multilayer_nonpixel_nopixel_tysh_enginedata_fillcolor_stylesheet_runlength_weighted_fillopacity_2run.psd",
        "snake16_psb_cmyk8_missing_composite_multilayer_nonpixel_nopixel_tysh_enginedata_fillcolor_stylesheet_runlength_weighted_fillopacity_2run.psd",
    )
    convert_fixture(
        "snake16_mode7_cmyk8_missing_composite_multilayer_nonpixel_nopixel_tysh_enginedata_fillcolor_stylesheet_runlength_weighted_fillopacity_2run.psd",
        "snake16_psb_mode7_cmyk8_missing_composite_multilayer_nonpixel_nopixel_tysh_enginedata_fillcolor_stylesheet_runlength_weighted_fillopacity_2run.psd",
    )
    convert_fixture(
        "snake16_mode7_cmyk8_missing_composite_multilayer_nonpixel_nopixel_tysh_enginedata_fillcolor_stylesheet_runlength_weighted_fillopacity_malformed.psd",
        "snake16_psb_mode7_cmyk8_missing_composite_multilayer_nonpixel_nopixel_tysh_enginedata_fillcolor_stylesheet_runlength_weighted_fillopacity_malformed.psd",
    )
    convert_fixture(
        "snake16_cmyk8_missing_composite_multilayer_nonpixel_nopixel_tysh_enginedata_fillcolor_fillflag_false_strokeflag_true.psd",
        "snake16_psb_cmyk8_missing_composite_multilayer_nonpixel_nopixel_tysh_enginedata_fillcolor_fillflag_false_strokeflag_true.psd",
    )
    convert_fixture(
        "snake16_mode7_cmyk8_missing_composite_multilayer_nonpixel_nopixel_tysh_enginedata_fillcolor_fillflag_false_strokeflag_true.psd",
        "snake16_psb_mode7_cmyk8_missing_composite_multilayer_nonpixel_nopixel_tysh_enginedata_fillcolor_fillflag_false_strokeflag_true.psd",
    )
    convert_fixture(
        "snake16_cmyk8_missing_composite_multilayer_nonpixel_nopixel_tysh_enginedata_fillcolor_fillflag_false_strokeflag_true_strokeopacity_050.psd",
        "snake16_psb_cmyk8_missing_composite_multilayer_nonpixel_nopixel_tysh_enginedata_fillcolor_fillflag_false_strokeflag_true_strokeopacity_050.psd",
    )
    convert_fixture(
        "snake16_mode7_cmyk8_missing_composite_multilayer_nonpixel_nopixel_tysh_enginedata_fillcolor_fillflag_false_strokeflag_true_strokeopacity_050.psd",
        "snake16_psb_mode7_cmyk8_missing_composite_multilayer_nonpixel_nopixel_tysh_enginedata_fillcolor_fillflag_false_strokeflag_true_strokeopacity_050.psd",
    )
    convert_fixture(
        "snake16_mode7_cmyk8_missing_composite_multilayer_nonpixel_nopixel_tysh_enginedata_fillcolor_fillflag_false_strokeflag_true_strokeopacity_malformed.psd",
        "snake16_psb_mode7_cmyk8_missing_composite_multilayer_nonpixel_nopixel_tysh_enginedata_fillcolor_fillflag_false_strokeflag_true_strokeopacity_malformed.psd",
    )
    convert_fixture(
        "snake16_cmyk8_missing_composite_multilayer_nonpixel_nopixel_tysh_enginedata_fillcolor_fillflag_false_strokeflag_true_wso_2run.psd",
        "snake16_psb_cmyk8_missing_composite_multilayer_nonpixel_nopixel_tysh_enginedata_fillcolor_fillflag_false_strokeflag_true_wso_2run.psd",
    )
    convert_fixture(
        "snake16_mode7_cmyk8_missing_composite_multilayer_nonpixel_nopixel_tysh_enginedata_fillcolor_fillflag_false_strokeflag_true_wso_2run.psd",
        "snake16_psb_mode7_cmyk8_missing_composite_multilayer_nonpixel_nopixel_tysh_enginedata_fillcolor_fillflag_false_strokeflag_true_wso_2run.psd",
    )
    convert_fixture(
        "snake16_mode7_cmyk8_missing_composite_multilayer_nonpixel_nopixel_tysh_enginedata_fillcolor_fillflag_false_strokeflag_true_wso_malformed.psd",
        "snake16_psb_mode7_cmyk8_missing_composite_multilayer_nonpixel_nopixel_tysh_enginedata_fillcolor_fillflag_false_strokeflag_true_wso_malformed.psd",
    )
    convert_fixture(
        "snake16_cmyk8_missing_composite_multilayer_nonpixel_nopixel_tysh_enginedata_fillcolor_fillflag_false_strokeflag_false.psd",
        "snake16_psb_cmyk8_missing_composite_multilayer_nonpixel_nopixel_tysh_enginedata_fillcolor_fillflag_false_strokeflag_false.psd",
    )
    convert_fixture(
        "snake16_mode7_cmyk8_missing_composite_multilayer_nonpixel_nopixel_tysh_enginedata_fillcolor_fillflag_false_strokeflag_false.psd",
        "snake16_psb_mode7_cmyk8_missing_composite_multilayer_nonpixel_nopixel_tysh_enginedata_fillcolor_fillflag_false_strokeflag_false.psd",
    )
    convert_fixture(
        "snake16_cmyk8_missing_composite_multilayer_nonpixel_nopixel_tysh_enginedata_strokecolor_stylesheet_runlength_weighted_2run.psd",
        "snake16_psb_cmyk8_missing_composite_multilayer_nonpixel_nopixel_tysh_enginedata_strokecolor_stylesheet_runlength_weighted_2run.psd",
    )
    convert_fixture(
        "snake16_mode7_cmyk8_missing_composite_multilayer_nonpixel_nopixel_tysh_enginedata_strokecolor_stylesheet_runlength_weighted_3run.psd",
        "snake16_psb_mode7_cmyk8_missing_composite_multilayer_nonpixel_nopixel_tysh_enginedata_strokecolor_stylesheet_runlength_weighted_3run.psd",
    )
    convert_fixture(
        "snake16_cmyk8_missing_composite_multilayer_nonpixel_nopixel_tysh_enginedata_fillcolor_values_cmyk.psd",
        "snake16_psb_cmyk8_missing_composite_multilayer_nonpixel_nopixel_tysh_enginedata_fillcolor_values_cmyk.psd",
    )
    convert_fixture(
        "snake16_cmyk8_missing_composite_multilayer_nonpixel_nopixel_tysh_enginedata_fillcolor_values_named_cmyk.psd",
        "snake16_psb_cmyk8_missing_composite_multilayer_nonpixel_nopixel_tysh_enginedata_fillcolor_values_named_cmyk.psd",
    )
    convert_fixture(
        "snake16_mode7_cmyk8_missing_composite_multilayer_nonpixel_nopixel_tysh_enginedata_fillcolor_values_cmyk.psd",
        "snake16_psb_mode7_cmyk8_missing_composite_multilayer_nonpixel_nopixel_tysh_enginedata_fillcolor_values_cmyk.psd",
    )
    convert_fixture(
        "snake16_mode7_cmyk8_missing_composite_multilayer_nonpixel_nopixel_tysh_enginedata_fillcolor_values_named_cmyk.psd",
        "snake16_psb_mode7_cmyk8_missing_composite_multilayer_nonpixel_nopixel_tysh_enginedata_fillcolor_values_named_cmyk.psd",
    )
    convert_fixture(
        "snake16_cmyk8_missing_composite_multilayer_nonpixel_nopixel_tysh_enginedata_fillcolor_values_gray.psd",
        "snake16_psb_cmyk8_missing_composite_multilayer_nonpixel_nopixel_tysh_enginedata_fillcolor_values_gray.psd",
    )
    convert_fixture(
        "snake16_mode7_cmyk8_missing_composite_multilayer_nonpixel_nopixel_tysh_enginedata_fillcolor_values_gray.psd",
        "snake16_psb_mode7_cmyk8_missing_composite_multilayer_nonpixel_nopixel_tysh_enginedata_fillcolor_values_gray.psd",
    )
    convert_fixture(
        "snake16_mode7_cmyk32_missing_composite_multilayer_nonpixel_nopixel_tysh_enginedata_fillcolor_stylesheet_runlength_precedence.psd",
        "snake16_psb_mode7_cmyk32_missing_composite_multilayer_nonpixel_nopixel_tysh_enginedata_fillcolor_stylesheet_runlength_precedence.psd",
    )
    convert_fixture(
        "snake16_mode7_cmyk8_missing_composite_multilayer_nonpixel_nopixel_tysh_enginedata_fillcolor_stylesheetset_runlength_precedence.psd",
        "snake16_psb_mode7_cmyk8_missing_composite_multilayer_nonpixel_nopixel_tysh_enginedata_fillcolor_stylesheetset_runlength_precedence.psd",
    )
    convert_fixture(
        "snake16_cmyk8_missing_composite_multilayer_nonpixel_nopixel_tysh_enginedata_fillcolor_stylesheetset_runstyle_precedence.psd",
        "snake16_psb_cmyk8_missing_composite_multilayer_nonpixel_nopixel_tysh_enginedata_fillcolor_stylesheetset_runstyle_precedence.psd",
    )
    convert_fixture(
        "snake16_mode7_cmyk8_missing_composite_multilayer_nonpixel_nopixel_tysh_enginedata_fillcolor_stylesheetset_runstyle_precedence.psd",
        "snake16_psb_mode7_cmyk8_missing_composite_multilayer_nonpixel_nopixel_tysh_enginedata_fillcolor_stylesheetset_runstyle_precedence.psd",
    )
    convert_fixture(
        "snake16_cmyk8_missing_composite_multilayer_nonpixel_nopixel_tysh_enginedata_fillcolor_default_stylesheet_values_rgb.psd",
        "snake16_psb_cmyk8_missing_composite_multilayer_nonpixel_nopixel_tysh_enginedata_fillcolor_default_stylesheet_values_rgb.psd",
    )
    convert_fixture(
        "snake16_mode7_cmyk8_missing_composite_multilayer_nonpixel_nopixel_tysh_enginedata_fillcolor_default_stylesheet_values_rgb.psd",
        "snake16_psb_mode7_cmyk8_missing_composite_multilayer_nonpixel_nopixel_tysh_enginedata_fillcolor_default_stylesheet_values_rgb.psd",
    )
    convert_fixture(
        "snake16_cmyk8_missing_composite_multilayer_nonpixel_nopixel_tysh_enginedata_fillcolor_default_stylesheet_malformed_payload.psd",
        "snake16_psb_cmyk8_missing_composite_multilayer_nonpixel_nopixel_tysh_enginedata_fillcolor_default_stylesheet_malformed_payload.psd",
    )
    convert_fixture(
        "snake16_mode7_cmyk8_missing_composite_multilayer_nonpixel_nopixel_tysh_enginedata_fillcolor_default_stylesheet_malformed_payload.psd",
        "snake16_psb_mode7_cmyk8_missing_composite_multilayer_nonpixel_nopixel_tysh_enginedata_fillcolor_default_stylesheet_malformed_payload.psd",
    )
    convert_fixture(
        "snake16_cmyk16_missing_composite_multilayer_nonpixel_nopixel_tysh_enginedata_fillcolor_stylesheetset_runstyle_precedence.psd",
        "snake16_psb_cmyk16_missing_composite_multilayer_nonpixel_nopixel_tysh_enginedata_fillcolor_stylesheetset_runstyle_precedence.psd",
    )
    convert_fixture(
        "snake16_mode7_cmyk32_missing_composite_multilayer_nonpixel_nopixel_tysh_enginedata_fillcolor_stylesheetset_runstyle_precedence.psd",
        "snake16_psb_mode7_cmyk32_missing_composite_multilayer_nonpixel_nopixel_tysh_enginedata_fillcolor_stylesheetset_runstyle_precedence.psd",
    )
    convert_fixture(
        "snake16_cmyk8_missing_composite_multilayer_nonpixel_nopixel_tysh_enginedata_fillcolor_stylesheet_values_cmyk.psd",
        "snake16_psb_cmyk8_missing_composite_multilayer_nonpixel_nopixel_tysh_enginedata_fillcolor_stylesheet_values_cmyk.psd",
    )
    convert_fixture(
        "snake16_mode7_cmyk8_missing_composite_multilayer_nonpixel_nopixel_tysh_enginedata_fillcolor_stylesheet_values_cmyk.psd",
        "snake16_psb_mode7_cmyk8_missing_composite_multilayer_nonpixel_nopixel_tysh_enginedata_fillcolor_stylesheet_values_cmyk.psd",
    )
    convert_fixture(
        "snake16_cmyk8_missing_composite_multilayer_nonpixel_nopixel_tysh_enginedata_fillcolor_stylesheet_values_lab.psd",
        "snake16_psb_cmyk8_missing_composite_multilayer_nonpixel_nopixel_tysh_enginedata_fillcolor_stylesheet_values_lab.psd",
    )
    convert_fixture(
        "snake16_mode7_cmyk8_missing_composite_multilayer_nonpixel_nopixel_tysh_enginedata_fillcolor_stylesheet_values_lab.psd",
        "snake16_psb_mode7_cmyk8_missing_composite_multilayer_nonpixel_nopixel_tysh_enginedata_fillcolor_stylesheet_values_lab.psd",
    )
    convert_fixture(
        "snake16_cmyk8_missing_composite_multilayer_nonpixel_nopixel_tysh_enginedata_fillcolor_rgb_valid_icc_profile.psd",
        "snake16_psb_cmyk8_missing_composite_multilayer_nonpixel_nopixel_tysh_enginedata_fillcolor_rgb_valid_icc_profile.psd",
    )
    convert_fixture(
        "snake16_mode7_cmyk8_missing_composite_multilayer_nonpixel_nopixel_tysh_enginedata_fillcolor_rgb_valid_icc_profile.psd",
        "snake16_psb_mode7_cmyk8_missing_composite_multilayer_nonpixel_nopixel_tysh_enginedata_fillcolor_rgb_valid_icc_profile.psd",
    )
    convert_fixture(
        "snake16_cmyk8_missing_composite_multilayer_nonpixel_nopixel_tysh_enginedata_fillcolor_values_cmyk_valid_icc_profile.psd",
        "snake16_psb_cmyk8_missing_composite_multilayer_nonpixel_nopixel_tysh_enginedata_fillcolor_values_cmyk_valid_icc_profile.psd",
    )
    convert_fixture(
        "snake16_mode7_cmyk8_missing_composite_multilayer_nonpixel_nopixel_tysh_enginedata_fillcolor_values_cmyk_valid_icc_profile.psd",
        "snake16_psb_mode7_cmyk8_missing_composite_multilayer_nonpixel_nopixel_tysh_enginedata_fillcolor_values_cmyk_valid_icc_profile.psd",
    )
    convert_fixture(
        "snake16_cmyk8_missing_composite_multilayer_nonpixel_nopixel_tysh_enginedata_fillcolor_values_gray_valid_icc_profile.psd",
        "snake16_psb_cmyk8_missing_composite_multilayer_nonpixel_nopixel_tysh_enginedata_fillcolor_values_gray_valid_icc_profile.psd",
    )
    convert_fixture(
        "snake16_mode7_cmyk8_missing_composite_multilayer_nonpixel_nopixel_tysh_enginedata_fillcolor_values_gray_valid_icc_profile.psd",
        "snake16_psb_mode7_cmyk8_missing_composite_multilayer_nonpixel_nopixel_tysh_enginedata_fillcolor_values_gray_valid_icc_profile.psd",
    )
    convert_fixture(
        "snake16_cmyk8_missing_composite_multilayer_nonpixel_nopixel_tysh_enginedata_fillcolor_stylesheet_values_cmyk_valid_icc_profile.psd",
        "snake16_psb_cmyk8_missing_composite_multilayer_nonpixel_nopixel_tysh_enginedata_fillcolor_stylesheet_values_cmyk_valid_icc_profile.psd",
    )
    convert_fixture(
        "snake16_mode7_cmyk8_missing_composite_multilayer_nonpixel_nopixel_tysh_enginedata_fillcolor_stylesheet_values_cmyk_valid_icc_profile.psd",
        "snake16_psb_mode7_cmyk8_missing_composite_multilayer_nonpixel_nopixel_tysh_enginedata_fillcolor_stylesheet_values_cmyk_valid_icc_profile.psd",
    )
    convert_fixture(
        "snake16_cmyk8_missing_composite_multilayer_nonpixel_nopixel_tysh_enginedata_fillcolor_stylesheet_values_lab_valid_icc_profile.psd",
        "snake16_psb_cmyk8_missing_composite_multilayer_nonpixel_nopixel_tysh_enginedata_fillcolor_stylesheet_values_lab_valid_icc_profile.psd",
    )
    convert_fixture(
        "snake16_mode7_cmyk8_missing_composite_multilayer_nonpixel_nopixel_tysh_enginedata_fillcolor_stylesheet_values_lab_valid_icc_profile.psd",
        "snake16_psb_mode7_cmyk8_missing_composite_multilayer_nonpixel_nopixel_tysh_enginedata_fillcolor_stylesheet_values_lab_valid_icc_profile.psd",
    )
    convert_fixture(
        "snake16_cmyk8_missing_composite_multilayer_nonpixel_nopixel_tysh_enginedata_fillcolor_rgb_bad_icc_profile.psd",
        "snake16_psb_cmyk8_missing_composite_multilayer_nonpixel_nopixel_tysh_enginedata_fillcolor_rgb_bad_icc_profile.psd",
    )
    convert_fixture(
        "snake16_mode7_cmyk8_missing_composite_multilayer_nonpixel_nopixel_tysh_enginedata_fillcolor_rgb_bad_icc_profile.psd",
        "snake16_psb_mode7_cmyk8_missing_composite_multilayer_nonpixel_nopixel_tysh_enginedata_fillcolor_rgb_bad_icc_profile.psd",
    )
    convert_fixture(
        "snake16_cmyk8_missing_composite_multilayer_nonpixel_nopixel_tysh_enginedata_fillcolor_values_cmyk_bad_icc_profile.psd",
        "snake16_psb_cmyk8_missing_composite_multilayer_nonpixel_nopixel_tysh_enginedata_fillcolor_values_cmyk_bad_icc_profile.psd",
    )
    convert_fixture(
        "snake16_cmyk8_missing_composite_multilayer_nonpixel_nopixel_tysh_enginedata_fillcolor_values_named_cmyk_bad_icc_profile.psd",
        "snake16_psb_cmyk8_missing_composite_multilayer_nonpixel_nopixel_tysh_enginedata_fillcolor_values_named_cmyk_bad_icc_profile.psd",
    )
    convert_fixture(
        "snake16_mode7_cmyk8_missing_composite_multilayer_nonpixel_nopixel_tysh_enginedata_fillcolor_values_cmyk_bad_icc_profile.psd",
        "snake16_psb_mode7_cmyk8_missing_composite_multilayer_nonpixel_nopixel_tysh_enginedata_fillcolor_values_cmyk_bad_icc_profile.psd",
    )
    convert_fixture(
        "snake16_mode7_cmyk8_missing_composite_multilayer_nonpixel_nopixel_tysh_enginedata_fillcolor_values_named_cmyk_bad_icc_profile.psd",
        "snake16_psb_mode7_cmyk8_missing_composite_multilayer_nonpixel_nopixel_tysh_enginedata_fillcolor_values_named_cmyk_bad_icc_profile.psd",
    )
    convert_fixture(
        "snake16_cmyk8_missing_composite_multilayer_nonpixel_nopixel_tysh_enginedata_fillcolor_values_gray_bad_icc_profile.psd",
        "snake16_psb_cmyk8_missing_composite_multilayer_nonpixel_nopixel_tysh_enginedata_fillcolor_values_gray_bad_icc_profile.psd",
    )
    convert_fixture(
        "snake16_mode7_cmyk8_missing_composite_multilayer_nonpixel_nopixel_tysh_enginedata_fillcolor_values_gray_bad_icc_profile.psd",
        "snake16_psb_mode7_cmyk8_missing_composite_multilayer_nonpixel_nopixel_tysh_enginedata_fillcolor_values_gray_bad_icc_profile.psd",
    )
    convert_fixture(
        "snake16_cmyk8_missing_composite_multilayer_nonpixel_nopixel_tysh_enginedata_fillcolor_stylesheet_values_cmyk_bad_icc_profile.psd",
        "snake16_psb_cmyk8_missing_composite_multilayer_nonpixel_nopixel_tysh_enginedata_fillcolor_stylesheet_values_cmyk_bad_icc_profile.psd",
    )
    convert_fixture(
        "snake16_mode7_cmyk8_missing_composite_multilayer_nonpixel_nopixel_tysh_enginedata_fillcolor_stylesheet_values_cmyk_bad_icc_profile.psd",
        "snake16_psb_mode7_cmyk8_missing_composite_multilayer_nonpixel_nopixel_tysh_enginedata_fillcolor_stylesheet_values_cmyk_bad_icc_profile.psd",
    )
    convert_fixture(
        "snake16_cmyk8_missing_composite_multilayer_nonpixel_nopixel_tysh_enginedata_fillcolor_stylesheet_values_gray_bad_icc_profile.psd",
        "snake16_psb_cmyk8_missing_composite_multilayer_nonpixel_nopixel_tysh_enginedata_fillcolor_stylesheet_values_gray_bad_icc_profile.psd",
    )
    convert_fixture(
        "snake16_mode7_cmyk8_missing_composite_multilayer_nonpixel_nopixel_tysh_enginedata_fillcolor_stylesheet_values_gray_bad_icc_profile.psd",
        "snake16_psb_mode7_cmyk8_missing_composite_multilayer_nonpixel_nopixel_tysh_enginedata_fillcolor_stylesheet_values_gray_bad_icc_profile.psd",
    )
    convert_fixture(
        "snake16_cmyk8_missing_composite_multilayer_nonpixel_nopixel_tysh_enginedata_fillcolor_stylesheet_values_lab_bad_icc_profile.psd",
        "snake16_psb_cmyk8_missing_composite_multilayer_nonpixel_nopixel_tysh_enginedata_fillcolor_stylesheet_values_lab_bad_icc_profile.psd",
    )
    convert_fixture(
        "snake16_mode7_cmyk8_missing_composite_multilayer_nonpixel_nopixel_tysh_enginedata_fillcolor_stylesheet_values_lab_bad_icc_profile.psd",
        "snake16_psb_mode7_cmyk8_missing_composite_multilayer_nonpixel_nopixel_tysh_enginedata_fillcolor_stylesheet_values_lab_bad_icc_profile.psd",
    )
    convert_fixture(
        "snake16_cmyk8_missing_composite_multilayer_nonpixel_nopixel_tysh_enginedata_fillcolor_rgb_malformed_resource.psd",
        "snake16_psb_cmyk8_missing_composite_multilayer_nonpixel_nopixel_tysh_enginedata_fillcolor_rgb_malformed_resource.psd",
    )
    convert_fixture(
        "snake16_mode7_cmyk8_missing_composite_multilayer_nonpixel_nopixel_tysh_enginedata_fillcolor_rgb_malformed_resource.psd",
        "snake16_psb_mode7_cmyk8_missing_composite_multilayer_nonpixel_nopixel_tysh_enginedata_fillcolor_rgb_malformed_resource.psd",
    )
    convert_fixture(
        "snake16_cmyk8_missing_composite_multilayer_nonpixel_nopixel_tysh_enginedata_fillcolor_values_cmyk_malformed_resource.psd",
        "snake16_psb_cmyk8_missing_composite_multilayer_nonpixel_nopixel_tysh_enginedata_fillcolor_values_cmyk_malformed_resource.psd",
    )
    convert_fixture(
        "snake16_cmyk8_missing_composite_multilayer_nonpixel_nopixel_tysh_enginedata_fillcolor_values_named_cmyk_malformed_resource.psd",
        "snake16_psb_cmyk8_missing_composite_multilayer_nonpixel_nopixel_tysh_enginedata_fillcolor_values_named_cmyk_malformed_resource.psd",
    )
    convert_fixture(
        "snake16_mode7_cmyk8_missing_composite_multilayer_nonpixel_nopixel_tysh_enginedata_fillcolor_values_cmyk_malformed_resource.psd",
        "snake16_psb_mode7_cmyk8_missing_composite_multilayer_nonpixel_nopixel_tysh_enginedata_fillcolor_values_cmyk_malformed_resource.psd",
    )
    convert_fixture(
        "snake16_mode7_cmyk8_missing_composite_multilayer_nonpixel_nopixel_tysh_enginedata_fillcolor_values_named_cmyk_malformed_resource.psd",
        "snake16_psb_mode7_cmyk8_missing_composite_multilayer_nonpixel_nopixel_tysh_enginedata_fillcolor_values_named_cmyk_malformed_resource.psd",
    )
    convert_fixture(
        "snake16_cmyk8_missing_composite_multilayer_nonpixel_nopixel_tysh_enginedata_fillcolor_values_gray_malformed_resource.psd",
        "snake16_psb_cmyk8_missing_composite_multilayer_nonpixel_nopixel_tysh_enginedata_fillcolor_values_gray_malformed_resource.psd",
    )
    convert_fixture(
        "snake16_mode7_cmyk8_missing_composite_multilayer_nonpixel_nopixel_tysh_enginedata_fillcolor_values_gray_malformed_resource.psd",
        "snake16_psb_mode7_cmyk8_missing_composite_multilayer_nonpixel_nopixel_tysh_enginedata_fillcolor_values_gray_malformed_resource.psd",
    )
    convert_fixture(
        "snake16_cmyk8_missing_composite_multilayer_nonpixel_nopixel_tysh_enginedata_fillcolor_stylesheet_values_cmyk_malformed_resource.psd",
        "snake16_psb_cmyk8_missing_composite_multilayer_nonpixel_nopixel_tysh_enginedata_fillcolor_stylesheet_values_cmyk_malformed_resource.psd",
    )
    convert_fixture(
        "snake16_mode7_cmyk8_missing_composite_multilayer_nonpixel_nopixel_tysh_enginedata_fillcolor_stylesheet_values_cmyk_malformed_resource.psd",
        "snake16_psb_mode7_cmyk8_missing_composite_multilayer_nonpixel_nopixel_tysh_enginedata_fillcolor_stylesheet_values_cmyk_malformed_resource.psd",
    )
    convert_fixture(
        "snake16_cmyk8_missing_composite_multilayer_nonpixel_nopixel_tysh_enginedata_fillcolor_stylesheet_values_gray_malformed_resource.psd",
        "snake16_psb_cmyk8_missing_composite_multilayer_nonpixel_nopixel_tysh_enginedata_fillcolor_stylesheet_values_gray_malformed_resource.psd",
    )
    convert_fixture(
        "snake16_mode7_cmyk8_missing_composite_multilayer_nonpixel_nopixel_tysh_enginedata_fillcolor_stylesheet_values_gray_malformed_resource.psd",
        "snake16_psb_mode7_cmyk8_missing_composite_multilayer_nonpixel_nopixel_tysh_enginedata_fillcolor_stylesheet_values_gray_malformed_resource.psd",
    )
    convert_fixture(
        "snake16_cmyk8_missing_composite_multilayer_nonpixel_nopixel_tysh_enginedata_fillcolor_stylesheet_values_lab_malformed_resource.psd",
        "snake16_psb_cmyk8_missing_composite_multilayer_nonpixel_nopixel_tysh_enginedata_fillcolor_stylesheet_values_lab_malformed_resource.psd",
    )
    convert_fixture(
        "snake16_mode7_cmyk8_missing_composite_multilayer_nonpixel_nopixel_tysh_enginedata_fillcolor_stylesheet_values_lab_malformed_resource.psd",
        "snake16_psb_mode7_cmyk8_missing_composite_multilayer_nonpixel_nopixel_tysh_enginedata_fillcolor_stylesheet_values_lab_malformed_resource.psd",
    )
    convert_fixture(
        "snake16_mode7_cmyk16_missing_composite_multilayer_nonpixel_nopixel_tysh_descriptor_gray.psd",
        "snake16_psb_mode7_cmyk16_missing_composite_multilayer_nonpixel_nopixel_tysh_descriptor_gray.psd",
    )
    convert_fixture(
        "snake16_mode7_cmyk16_missing_composite_multilayer_nonpixel_nopixel_tysh_descriptor_cmyk.psd",
        "snake16_psb_mode7_cmyk16_missing_composite_multilayer_nonpixel_nopixel_tysh_descriptor_cmyk.psd",
    )
    convert_fixture(
        "snake16_mode7_cmyk16_missing_composite_multilayer_nonpixel_nopixel_tysh_descriptor_hsb.psd",
        "snake16_psb_mode7_cmyk16_missing_composite_multilayer_nonpixel_nopixel_tysh_descriptor_hsb.psd",
    )
    convert_fixture(
        "snake16_mode7_cmyk16_missing_composite_multilayer_nonpixel_nopixel_tysh_descriptor_lab.psd",
        "snake16_psb_mode7_cmyk16_missing_composite_multilayer_nonpixel_nopixel_tysh_descriptor_lab.psd",
    )
    convert_fixture(
        "snake16_mode7_cmyk32_missing_composite_multilayer_nonpixel_nopixel_tysh_descriptor_gray.psd",
        "snake16_psb_mode7_cmyk32_missing_composite_multilayer_nonpixel_nopixel_tysh_descriptor_gray.psd",
    )
    convert_fixture(
        "snake16_mode7_cmyk32_missing_composite_multilayer_nonpixel_nopixel_tysh_descriptor_cmyk.psd",
        "snake16_psb_mode7_cmyk32_missing_composite_multilayer_nonpixel_nopixel_tysh_descriptor_cmyk.psd",
    )
    convert_fixture(
        "snake16_mode7_cmyk32_missing_composite_multilayer_nonpixel_nopixel_tysh_descriptor_hsb.psd",
        "snake16_psb_mode7_cmyk32_missing_composite_multilayer_nonpixel_nopixel_tysh_descriptor_hsb.psd",
    )
    convert_fixture(
        "snake16_mode7_cmyk32_missing_composite_multilayer_nonpixel_nopixel_tysh_descriptor_lab.psd",
        "snake16_psb_mode7_cmyk32_missing_composite_multilayer_nonpixel_nopixel_tysh_descriptor_lab.psd",
    )
    convert_fixture(
        "snake16_mode7_cmyk8_missing_composite_multilayer_nonpixel_nopixel_tysh_wrapped_descriptor.psd",
        "snake16_psb_mode7_cmyk8_missing_composite_multilayer_nonpixel_nopixel_tysh_wrapped_descriptor.psd",
    )
    convert_fixture(
        "snake16_mode7_cmyk8_missing_composite_multilayer_nonpixel_nopixel_tysh_wrapped_unknown_descriptor.psd",
        "snake16_psb_mode7_cmyk8_missing_composite_multilayer_nonpixel_nopixel_tysh_wrapped_unknown_descriptor.psd",
    )
    convert_fixture(
        "snake16_mode7_cmyk8_missing_composite_multilayer_nonpixel_nopixel_tysh_wrapped_malformed_descriptor.psd",
        "snake16_psb_mode7_cmyk8_missing_composite_multilayer_nonpixel_nopixel_tysh_wrapped_malformed_descriptor.psd",
    )
    convert_fixture(
        "snake16_mode7_cmyk16_missing_composite_multilayer_nonpixel_nopixel_tysh_wrapped_descriptor.psd",
        "snake16_psb_mode7_cmyk16_missing_composite_multilayer_nonpixel_nopixel_tysh_wrapped_descriptor.psd",
    )
    convert_fixture(
        "snake16_mode7_cmyk16_missing_composite_multilayer_nonpixel_nopixel_tysh_wrapped_unknown_descriptor.psd",
        "snake16_psb_mode7_cmyk16_missing_composite_multilayer_nonpixel_nopixel_tysh_wrapped_unknown_descriptor.psd",
    )
    convert_fixture(
        "snake16_mode7_cmyk16_missing_composite_multilayer_nonpixel_nopixel_tysh_wrapped_malformed_descriptor.psd",
        "snake16_psb_mode7_cmyk16_missing_composite_multilayer_nonpixel_nopixel_tysh_wrapped_malformed_descriptor.psd",
    )
    convert_fixture(
        "snake16_mode7_cmyk32_missing_composite_multilayer_nonpixel_nopixel_tysh_wrapped_descriptor.psd",
        "snake16_psb_mode7_cmyk32_missing_composite_multilayer_nonpixel_nopixel_tysh_wrapped_descriptor.psd",
    )
    convert_fixture(
        "snake16_mode7_cmyk32_missing_composite_multilayer_nonpixel_nopixel_tysh_wrapped_unknown_descriptor.psd",
        "snake16_psb_mode7_cmyk32_missing_composite_multilayer_nonpixel_nopixel_tysh_wrapped_unknown_descriptor.psd",
    )
    convert_fixture(
        "snake16_mode7_cmyk32_missing_composite_multilayer_nonpixel_nopixel_tysh_wrapped_malformed_descriptor.psd",
        "snake16_psb_mode7_cmyk32_missing_composite_multilayer_nonpixel_nopixel_tysh_wrapped_malformed_descriptor.psd",
    )
    icc_valid_suffixes = [
        "normal",
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
    tysh_color_values_named_suffixes = [
        "nonpixel_nopixel_tysh_enginedata_fillcolor_color_values_named_device_cmyk",
        "nonpixel_nopixel_tysh_enginedata_fillcolor_color_values_named_device_rgb",
        "nonpixel_nopixel_tysh_enginedata_fillcolor_color_values_named_device_gray",
        "nonpixel_nopixel_tysh_enginedata_fillcolor_color_values_named_cielab",
    ]
    for mode_prefix in ("cmyk", "mode7_cmyk"):
        for depth_tag in ("8", "16", "32"):
            src_base = f"snake16_{mode_prefix}{depth_tag}_missing_composite_multilayer"
            dst_base = f"snake16_psb_{mode_prefix}{depth_tag}_missing_composite_multilayer"
            for suffix in [
                "nonpixel_nopixel_tysh_enginedata_fillcolor_values_named_cmyk",
                "nonpixel_nopixel_tysh_enginedata_fillcolor_values_named_hsb",
                "nonpixel_nopixel_tysh_enginedata_fillcolor_values_named_device_rgb",
                "nonpixel_nopixel_tysh_enginedata_fillcolor_values_named_device_cmyk",
                "nonpixel_nopixel_tysh_enginedata_fillcolor_values_named_device_gray",
                "nonpixel_nopixel_tysh_enginedata_fillcolor_values_named_object_device_rgb",
                "nonpixel_nopixel_tysh_enginedata_fillcolor_values_named_object_device_cmyk",
                "nonpixel_nopixel_tysh_enginedata_fillcolor_values_named_cielab",
                "nonpixel_nopixel_tysh_enginedata_fillcolor_stylesheet_values_cmyk",
                "nonpixel_nopixel_tysh_enginedata_fillcolor_stylesheet_values_named_hsb",
                "nonpixel_nopixel_tysh_enginedata_fillcolor_stylesheet_values_named_device_rgb",
                "nonpixel_nopixel_tysh_enginedata_fillcolor_stylesheet_values_named_device_cmyk",
                "nonpixel_nopixel_tysh_enginedata_fillcolor_stylesheet_values_named_device_gray",
                "nonpixel_nopixel_tysh_enginedata_fillcolor_stylesheet_values_named_object_device_rgb",
                "nonpixel_nopixel_tysh_enginedata_fillcolor_stylesheet_values_named_object_device_cmyk",
                "nonpixel_nopixel_tysh_enginedata_fillcolor_stylesheet_values_named_cielab",
                "nonpixel_nopixel_tysh_enginedata_fillcolor_stylesheet_values_gray",
                "nonpixel_nopixel_tysh_enginedata_fillcolor_stylesheet_values_lab",
                "nonpixel_nopixel_tysh_enginedata_fillcolor_dual_scope_precedence",
                "nonpixel_nopixel_tysh_enginedata_fillcolor_dual_scope_values_precedence",
                "nonpixel_nopixel_tysh_enginedata_fillcolor_dual_scope_nested_values_precedence",
            ]:
                convert_fixture(
                    f"{src_base}_{suffix}.psd",
                    f"{dst_base}_{suffix}.psd",
                )
            if depth_tag == "8":
                for suffix in [
                    "nonpixel_nopixel_tysh_enginedata_fillcolor_color_values_cmyk",
                    "nonpixel_nopixel_tysh_enginedata_fillcolor_color_values_named_device_cmyk",
                    "nonpixel_nopixel_tysh_enginedata_fillcolor_color_values_named_device_rgb",
                    "nonpixel_nopixel_tysh_enginedata_fillcolor_color_values_named_device_gray",
                    "nonpixel_nopixel_tysh_enginedata_fillcolor_color_values_named_cielab",
                    "nonpixel_nopixel_tysh_enginedata_fillcolor_color_values_named_device_rgb_malformed_payload",
                    "nonpixel_nopixel_tysh_enginedata_fillcolor_color_values_named_device_rgb_short_payload",
                    "nonpixel_nopixel_tysh_enginedata_fillcolor_stylesheet_values_named_device_rgb_short_payload",
                    "nonpixel_nopixel_tysh_enginedata_fillcolor_dual_scope_stylesheet_color_values_precedence",
                    "nonpixel_nopixel_tysh_enginedata_fillcolor_dual_scope_stylesheet_array_precedence",
                    "nonpixel_nopixel_tysh_enginedata_fillcolor_dual_scope_stylesheet_array_color_values_precedence",
                    "nonpixel_nopixel_tysh_enginedata_fillcolor_dual_stylesheet_precedence",
                    "nonpixel_nopixel_tysh_enginedata_fillcolor_stylesheet_runlength_precedence",
                    "nonpixel_nopixel_tysh_enginedata_fillcolor_stylesheet_runlength_weighted_2run",
                    "nonpixel_nopixel_tysh_enginedata_fillcolor_stylesheet_runlength_weighted_3run",
                    "nonpixel_nopixel_tysh_enginedata_fillcolor_stylesheetset_runstyle_runlength_weighted_2run",
                    "nonpixel_nopixel_tysh_enginedata_fillcolor_stylesheetset_runstyle_runlength_unresolved_continue",
                    "nonpixel_nopixel_tysh_enginedata_fillcolor_stylesheet_runlength_negative_continue",
                ]:
                    convert_fixture(
                        f"{src_base}_{suffix}.psd",
                        f"{dst_base}_{suffix}.psd",
                    )
                for suffix in [
                    "nonpixel_nopixel_tysh_enginedata_fillcolor_stylesheet_runlength_weighted_2run",
                    "nonpixel_nopixel_tysh_enginedata_fillcolor_stylesheet_runlength_weighted_3run",
                    "nonpixel_nopixel_tysh_enginedata_fillcolor_stylesheetset_runstyle_runlength_weighted_2run",
                    "nonpixel_nopixel_tysh_enginedata_fillcolor_stylesheetset_runstyle_runlength_unresolved_continue",
                    "nonpixel_nopixel_tysh_enginedata_fillcolor_stylesheet_runlength_negative_continue",
                    "nonpixel_nopixel_tysh_enginedata_fillcolor_fillopacity_050",
                    "nonpixel_nopixel_tysh_enginedata_fillcolor_fillflag_false_strokeflag_true_strokeopacity_050",
                ]:
                    convert_fixture(
                        f"{src_base}_{suffix}_valid_icc_profile.psd",
                        f"{dst_base}_{suffix}_valid_icc_profile.psd",
                    )
            for suffix in icc_valid_suffixes:
                convert_fixture(
                    f"{src_base}_{suffix}_valid_icc_profile.psd",
                    f"{dst_base}_{suffix}_valid_icc_profile.psd",
                )
            for suffix in [
                "normal",
                tysh_bad_rotation[depth_tag],
                fill_bad_rotation[depth_tag],
            ]:
                convert_fixture(
                    f"{src_base}_{suffix}_bad_icc_profile.psd",
                    f"{dst_base}_{suffix}_bad_icc_profile.psd",
                )
            if depth_tag == "8":
                for suffix in [
                    "nonpixel_nopixel_tysh_enginedata_fillcolor_values_named_hsb",
                    "nonpixel_nopixel_tysh_enginedata_fillcolor_stylesheet_values_named_hsb",
                    "nonpixel_nopixel_tysh_enginedata_fillcolor_stylesheet_runlength_weighted_2run",
                    "nonpixel_nopixel_tysh_enginedata_fillcolor_stylesheet_runlength_weighted_3run",
                    "nonpixel_nopixel_tysh_enginedata_fillcolor_stylesheetset_runstyle_runlength_weighted_2run",
                    "nonpixel_nopixel_tysh_enginedata_fillcolor_stylesheetset_runstyle_runlength_unresolved_continue",
                    "nonpixel_nopixel_tysh_enginedata_fillcolor_stylesheet_runlength_negative_continue",
                    "nonpixel_nopixel_tysh_enginedata_fillcolor_fillopacity_050",
                    "nonpixel_nopixel_tysh_enginedata_fillcolor_fillflag_false_strokeflag_true_strokeopacity_050",
                ]:
                    convert_fixture(
                        f"{src_base}_{suffix}_bad_icc_profile.psd",
                        f"{dst_base}_{suffix}_bad_icc_profile.psd",
                    )
                for suffix in tysh_color_values_named_suffixes:
                    convert_fixture(
                        f"{src_base}_{suffix}_bad_icc_profile.psd",
                        f"{dst_base}_{suffix}_bad_icc_profile.psd",
                    )
            for suffix in [
                "normal",
                tysh_malformed_rotation[depth_tag],
                fill_malformed_rotation[depth_tag],
            ]:
                convert_fixture(
                    f"{src_base}_{suffix}_malformed_resource.psd",
                    f"{dst_base}_{suffix}_malformed_resource.psd",
                )
            if depth_tag == "8":
                for suffix in [
                    "nonpixel_nopixel_tysh_enginedata_fillcolor_values_named_hsb",
                    "nonpixel_nopixel_tysh_enginedata_fillcolor_stylesheet_values_named_hsb",
                    "nonpixel_nopixel_tysh_enginedata_fillcolor_stylesheet_runlength_weighted_2run",
                    "nonpixel_nopixel_tysh_enginedata_fillcolor_stylesheet_runlength_weighted_3run",
                    "nonpixel_nopixel_tysh_enginedata_fillcolor_stylesheetset_runstyle_runlength_weighted_2run",
                    "nonpixel_nopixel_tysh_enginedata_fillcolor_stylesheetset_runstyle_runlength_unresolved_continue",
                    "nonpixel_nopixel_tysh_enginedata_fillcolor_stylesheet_runlength_negative_continue",
                    "nonpixel_nopixel_tysh_enginedata_fillcolor_fillopacity_050",
                    "nonpixel_nopixel_tysh_enginedata_fillcolor_fillflag_false_strokeflag_true_strokeopacity_050",
                ]:
                    convert_fixture(
                        f"{src_base}_{suffix}_malformed_resource.psd",
                        f"{dst_base}_{suffix}_malformed_resource.psd",
                    )
                for suffix in tysh_color_values_named_suffixes:
                    convert_fixture(
                        f"{src_base}_{suffix}_valid_icc_profile.psd",
                        f"{dst_base}_{suffix}_valid_icc_profile.psd",
                    )
                for suffix in tysh_color_values_named_suffixes:
                    convert_fixture(
                        f"{src_base}_{suffix}_malformed_resource.psd",
                        f"{dst_base}_{suffix}_malformed_resource.psd",
                    )
    # Representative depth-matrix expansion for TySh StyleRun weighted-run LSQA:
    # keep PSB conversion minimal (mode4 + mode7 one case each).
    convert_fixture(
        "snake16_cmyk16_missing_composite_multilayer_nonpixel_nopixel_tysh_enginedata_fillcolor_stylesheet_runlength_weighted_2run.psd",
        "snake16_psb_cmyk16_missing_composite_multilayer_nonpixel_nopixel_tysh_enginedata_fillcolor_stylesheet_runlength_weighted_2run.psd",
    )
    convert_fixture(
        "snake16_mode7_cmyk32_missing_composite_multilayer_nonpixel_nopixel_tysh_enginedata_fillcolor_stylesheet_runlength_weighted_2run.psd",
        "snake16_psb_mode7_cmyk32_missing_composite_multilayer_nonpixel_nopixel_tysh_enginedata_fillcolor_stylesheet_runlength_weighted_2run.psd",
    )
    # Representative DefaultStyleSheet /Color named-space expansion:
    # mode4 + mode7 one decode case each, plus minimal ICC-contract surfaces.
    convert_fixture(
        "snake16_cmyk16_missing_composite_multilayer_nonpixel_nopixel_tysh_enginedata_fillcolor_default_stylesheet_color_values_named_device_gray.psd",
        "snake16_psb_cmyk16_missing_composite_multilayer_nonpixel_nopixel_tysh_enginedata_fillcolor_default_stylesheet_color_values_named_device_gray.psd",
    )
    convert_fixture(
        "snake16_mode7_cmyk32_missing_composite_multilayer_nonpixel_nopixel_tysh_enginedata_fillcolor_default_stylesheet_color_values_named_cielab.psd",
        "snake16_psb_mode7_cmyk32_missing_composite_multilayer_nonpixel_nopixel_tysh_enginedata_fillcolor_default_stylesheet_color_values_named_cielab.psd",
    )
    convert_fixture(
        "snake16_mode7_cmyk16_missing_composite_multilayer_nonpixel_nopixel_tysh_enginedata_fillcolor_default_stylesheet_color_values_named_device_cmyk.psd",
        "snake16_psb_mode7_cmyk16_missing_composite_multilayer_nonpixel_nopixel_tysh_enginedata_fillcolor_default_stylesheet_color_values_named_device_cmyk.psd",
    )
    convert_fixture(
        "snake16_mode7_cmyk8_missing_composite_multilayer_nonpixel_nopixel_tysh_enginedata_fillcolor_default_stylesheet_color_values_named_device_cmyk_valid_icc_profile.psd",
        "snake16_psb_mode7_cmyk8_missing_composite_multilayer_nonpixel_nopixel_tysh_enginedata_fillcolor_default_stylesheet_color_values_named_device_cmyk_valid_icc_profile.psd",
    )
    convert_fixture(
        "snake16_cmyk8_missing_composite_multilayer_nonpixel_nopixel_tysh_enginedata_fillcolor_default_stylesheet_color_values_named_device_cmyk_malformed_resource.psd",
        "snake16_psb_cmyk8_missing_composite_multilayer_nonpixel_nopixel_tysh_enginedata_fillcolor_default_stylesheet_color_values_named_device_cmyk_malformed_resource.psd",
    )
    convert_fixture(
        "snake16_rgb8_missing_composite_multilayer_fill_soco_descriptor_malformed.psd",
        "snake16_psb_rgb8_missing_composite_multilayer_fill_soco_descriptor_malformed.psd",
    )
    convert_fixture(
        "snake16_rgb8_missing_composite_multilayer_fill_gdfl_descriptor_malformed.psd",
        "snake16_psb_rgb8_missing_composite_multilayer_fill_gdfl_descriptor_malformed.psd",
    )
    convert_fixture(
        "snake16_rgb8_missing_composite_multilayer_fill_ptfl_descriptor_malformed.psd",
        "snake16_psb_rgb8_missing_composite_multilayer_fill_ptfl_descriptor_malformed.psd",
    )
    convert_fixture(
        "snake16_rgb8_missing_composite_multilayer_fill_soco_descriptor_invalid_payload.psd",
        "snake16_psb_rgb8_missing_composite_multilayer_fill_soco_descriptor_invalid_payload.psd",
    )
    convert_fixture(
        "snake16_rgb8_missing_composite_multilayer_fill_gdfl_descriptor_invalid_payload.psd",
        "snake16_psb_rgb8_missing_composite_multilayer_fill_gdfl_descriptor_invalid_payload.psd",
    )
    convert_fixture(
        "snake16_rgb8_missing_composite_multilayer_fill_ptfl_descriptor_invalid_payload.psd",
        "snake16_psb_rgb8_missing_composite_multilayer_fill_ptfl_descriptor_invalid_payload.psd",
    )

    # RLE row-table(4-byte) path fixture for PSB layer channel decoding.
    convert_fixture(
        "snake16_rgb8_missing_composite_multilayer_normal.psd",
        "snake16_psb_rgb8_missing_composite_multilayer_normal_rle.psd",
        force_rle=True,
    )
    convert_fixture(
        "snake16_mode7_cmyk16_missing_composite_multilayer_normal.psd",
        "snake16_psb_mode7_cmyk16_missing_composite_multilayer_normal_rle.psd",
        force_rle=True,
    )
    convert_fixture(
        "snake16_cmyk16_missing_composite_multilayer_normal.psd",
        "snake16_psb_cmyk16_missing_composite_multilayer_normal_rle.psd",
        force_rle=True,
    )
    convert_fixture(
        "snake16_mode7_cmyk32_missing_composite_multilayer_normal.psd",
        "snake16_psb_mode7_cmyk32_missing_composite_multilayer_normal_rle.psd",
        force_rle=True,
    )
    convert_fixture(
        "snake16_cmyk32_missing_composite_multilayer_normal.psd",
        "snake16_psb_cmyk32_missing_composite_multilayer_normal_rle.psd",
        force_rle=True,
    )
    convert_fixture(
        "snake16_cmyk8_missing_composite_multilayer_normal.psd",
        "snake16_psb_cmyk8_missing_composite_multilayer_normal_rle.psd",
        force_rle=True,
    )
    convert_fixture(
        "snake16_mode7_cmyk8_missing_composite_multilayer_normal.psd",
        "snake16_psb_mode7_cmyk8_missing_composite_multilayer_normal_rle.psd",
        force_rle=True,
    )

    # Trace fixtures (unsupported/malformed).
    convert_fixture(
        "snake16_lab16_missing_composite_marker.psd",
        "snake16_psb_lab16_missing_composite_marker.psd",
    )
    write_truncated_fixture(
        "snake16_psb_rgb8_missing_composite_multilayer_normal.psd",
        "snake16_psb_rgb8_missing_composite_multilayer_normal_truncated.psd",
        drop_tail_bytes=17,
    )
    mutate_psb_layer_mask_length_overflow(
        "snake16_psb_rgb8_missing_composite_multilayer_normal.psd",
        "snake16_psb_rgb8_missing_composite_multilayer_layer_mask_length_overflow.psd",
    )
    mutate_psb_layer_info_length_overflow(
        "snake16_psb_rgb8_missing_composite_multilayer_normal.psd",
        "snake16_psb_rgb8_missing_composite_multilayer_layer_info_length_overflow.psd",
    )
    mutate_psb_channel_length_overflow(
        "snake16_psb_mode7_cmyk16_missing_composite_multilayer_normal.psd",
        "snake16_psb_mode7_cmyk16_missing_composite_multilayer_channel_length_overflow.psd",
    )
    mutate_psb_channel_length_overflow(
        "snake16_psb_cmyk8_missing_composite_multilayer_normal.psd",
        "snake16_psb_cmyk8_missing_composite_multilayer_channel_length_overflow.psd",
    )
    mutate_psb_channel_length_overflow(
        "snake16_psb_cmyk32_missing_composite_multilayer_normal.psd",
        "snake16_psb_cmyk32_missing_composite_multilayer_channel_length_overflow.psd",
    )
    mutate_psb_layer_mask_length_u64max(
        "snake16_psb_rgb8_missing_composite_multilayer_normal.psd",
        "snake16_psb_rgb8_missing_composite_multilayer_layer_mask_length_u64max.psd",
    )
    mutate_psb_layer_info_length_u64max(
        "snake16_psb_rgb8_missing_composite_multilayer_normal.psd",
        "snake16_psb_rgb8_missing_composite_multilayer_layer_info_length_u64max.psd",
    )
    mutate_psb_layer_mask_length_u64max(
        "snake16_psb_cmyk8_missing_composite_multilayer_normal.psd",
        "snake16_psb_cmyk8_missing_composite_multilayer_layer_mask_length_u64max.psd",
    )
    mutate_psb_layer_info_length_u64max(
        "snake16_psb_cmyk8_missing_composite_multilayer_normal.psd",
        "snake16_psb_cmyk8_missing_composite_multilayer_layer_info_length_u64max.psd",
    )
    mutate_psb_layer_mask_length_u64max(
        "snake16_psb_cmyk32_missing_composite_multilayer_normal.psd",
        "snake16_psb_cmyk32_missing_composite_multilayer_layer_mask_length_u64max.psd",
    )
    mutate_psb_layer_info_length_u64max(
        "snake16_psb_cmyk32_missing_composite_multilayer_normal.psd",
        "snake16_psb_cmyk32_missing_composite_multilayer_layer_info_length_u64max.psd",
    )
    mutate_psb_layer_mask_length_u64max(
        "snake16_psb_mode7_cmyk8_missing_composite_multilayer_nonpixel_nopixel_tysh_descriptor_gray.psd",
        "snake16_psb_mode7_cmyk8_missing_composite_multilayer_layer_mask_length_u64max.psd",
    )
    mutate_psb_layer_info_length_u64max(
        "snake16_psb_mode7_cmyk8_missing_composite_multilayer_nonpixel_nopixel_tysh_descriptor_gray.psd",
        "snake16_psb_mode7_cmyk8_missing_composite_multilayer_layer_info_length_u64max.psd",
    )
    mutate_psb_layer_mask_length_u64max(
        "snake16_psb_mode7_cmyk32_missing_composite_multilayer_normal.psd",
        "snake16_psb_mode7_cmyk32_missing_composite_multilayer_layer_mask_length_u64max.psd",
    )
    mutate_psb_layer_info_length_u64max(
        "snake16_psb_mode7_cmyk32_missing_composite_multilayer_normal.psd",
        "snake16_psb_mode7_cmyk32_missing_composite_multilayer_layer_info_length_u64max.psd",
    )
    mutate_psb_channel_length_u64max(
        "snake16_psb_mode7_cmyk16_missing_composite_multilayer_normal.psd",
        "snake16_psb_mode7_cmyk16_missing_composite_multilayer_channel_length_u64max.psd",
    )
    mutate_psb_channel_length_u64max(
        "snake16_psb_mode7_cmyk32_missing_composite_multilayer_normal.psd",
        "snake16_psb_mode7_cmyk32_missing_composite_multilayer_channel_length_u64max.psd",
    )
    mutate_psb_channel_length_u64max(
        "snake16_psb_cmyk8_missing_composite_multilayer_normal.psd",
        "snake16_psb_cmyk8_missing_composite_multilayer_channel_length_u64max.psd",
    )
    mutate_psb_channel_length_u64max(
        "snake16_psb_cmyk32_missing_composite_multilayer_normal.psd",
        "snake16_psb_cmyk32_missing_composite_multilayer_channel_length_u64max.psd",
    )
    mutate_psb_layer_info_end_overrun(
        "snake16_psb_cmyk32_missing_composite_multilayer_normal.psd",
        "snake16_psb_cmyk32_missing_composite_multilayer_layer_info_end_overrun.psd",
    )
    mutate_psb_layer_info_end_overrun(
        "snake16_psb_mode7_cmyk32_missing_composite_multilayer_normal.psd",
        "snake16_psb_mode7_cmyk32_missing_composite_multilayer_layer_info_end_overrun.psd",
    )
    mutate_psb_channel_window_overrun(
        "snake16_psb_cmyk32_missing_composite_multilayer_normal.psd",
        "snake16_psb_cmyk32_missing_composite_multilayer_channel_window_overrun.psd",
    )
    mutate_psb_channel_window_overrun(
        "snake16_psb_mode7_cmyk32_missing_composite_multilayer_normal.psd",
        "snake16_psb_mode7_cmyk32_missing_composite_multilayer_channel_window_overrun.psd",
    )
    mutate_psb_rle_row_table_too_short(
        "snake16_psb_rgb8_missing_composite_multilayer_normal_rle.psd",
        "snake16_psb_rgb8_missing_composite_multilayer_rle_row_table_too_short.psd",
    )
    mutate_psb_rle_row_length_overrun(
        "snake16_psb_rgb8_missing_composite_multilayer_normal_rle.psd",
        "snake16_psb_rgb8_missing_composite_multilayer_rle_row_length_overrun.psd",
    )
    mutate_psb_rle_row_table_too_short(
        "snake16_psb_mode7_cmyk16_missing_composite_multilayer_normal_rle.psd",
        "snake16_psb_mode7_cmyk16_missing_composite_multilayer_rle_row_table_too_short.psd",
    )
    mutate_psb_rle_row_length_overrun(
        "snake16_psb_mode7_cmyk16_missing_composite_multilayer_normal_rle.psd",
        "snake16_psb_mode7_cmyk16_missing_composite_multilayer_rle_row_length_overrun.psd",
    )
    mutate_psb_rle_row_table_too_short(
        "snake16_psb_mode7_cmyk32_missing_composite_multilayer_normal_rle.psd",
        "snake16_psb_mode7_cmyk32_missing_composite_multilayer_rle_row_table_too_short.psd",
    )
    mutate_psb_rle_row_length_overrun(
        "snake16_psb_mode7_cmyk32_missing_composite_multilayer_normal_rle.psd",
        "snake16_psb_mode7_cmyk32_missing_composite_multilayer_rle_row_length_overrun.psd",
    )
    mutate_psb_rle_payload_window_overrun(
        "snake16_psb_cmyk32_missing_composite_multilayer_normal_rle.psd",
        "snake16_psb_cmyk32_missing_composite_multilayer_rle_payload_window_overrun.psd",
    )
    mutate_psb_rle_payload_window_overrun(
        "snake16_psb_mode7_cmyk32_missing_composite_multilayer_normal_rle.psd",
        "snake16_psb_mode7_cmyk32_missing_composite_multilayer_rle_payload_window_overrun.psd",
    )
    mutate_psb_high_offset_valid(
        "snake16_psb_cmyk16_missing_composite_multilayer_normal.psd",
        "snake16_psb_cmyk16_missing_composite_multilayer_normal_high_offset.psd",
    )
    mutate_psb_high_offset_valid(
        "snake16_psb_mode7_cmyk16_missing_composite_multilayer_normal.psd",
        "snake16_psb_mode7_cmyk16_missing_composite_multilayer_normal_high_offset.psd",
    )
    mutate_psb_high_offset_valid(
        "snake16_psb_cmyk16_missing_composite_multilayer_normal.psd",
        "snake16_psb_cmyk16_missing_composite_multilayer_normal_high_offset_large.psd",
        padding_bytes=0x100000,
    )
    mutate_psb_high_offset_valid(
        "snake16_psb_mode7_cmyk16_missing_composite_multilayer_normal.psd",
        "snake16_psb_mode7_cmyk16_missing_composite_multilayer_normal_high_offset_large.psd",
        padding_bytes=0x100000,
    )
    mutate_psb_high_offset_valid(
        "snake16_psb_cmyk16_missing_composite_multilayer_normal.psd",
        "snake16_psb_cmyk16_missing_composite_multilayer_normal_high_offset_xlarge.psd",
        padding_bytes=0x200000,
    )
    mutate_psb_high_offset_valid(
        "snake16_psb_mode7_cmyk16_missing_composite_multilayer_normal.psd",
        "snake16_psb_mode7_cmyk16_missing_composite_multilayer_normal_high_offset_xlarge.psd",
        padding_bytes=0x200000,
    )
    mutate_psb_high_offset_valid(
        "snake16_psb_cmyk16_missing_composite_multilayer_normal.psd",
        "snake16_psb_cmyk16_missing_composite_multilayer_normal_high_offset_xxlarge.psd",
        padding_bytes=0x400000,
    )
    mutate_psb_high_offset_valid(
        "snake16_psb_mode7_cmyk16_missing_composite_multilayer_normal.psd",
        "snake16_psb_mode7_cmyk16_missing_composite_multilayer_normal_high_offset_xxlarge.psd",
        padding_bytes=0x400000,
    )
    mutate_psb_high_offset_valid(
        "snake16_psb_cmyk16_missing_composite_multilayer_normal.psd",
        "snake16_psb_cmyk16_missing_composite_multilayer_normal_high_offset_xxxlarge.psd",
        padding_bytes=0x800000,
    )
    mutate_psb_high_offset_valid(
        "snake16_psb_cmyk16_missing_composite_multilayer_normal.psd",
        "snake16_psb_cmyk16_missing_composite_multilayer_normal_high_offset_xxxxlarge.psd",
        padding_bytes=0x1000000,
    )
    mutate_psb_high_offset_valid(
        "snake16_psb_cmyk16_missing_composite_multilayer_normal.psd",
        "snake16_psb_cmyk16_missing_composite_multilayer_normal_high_offset_xxxxxlarge.psd",
        padding_bytes=0x2000000,
    )
    mutate_psb_high_offset_valid(
        "snake16_psb_mode7_cmyk16_missing_composite_multilayer_normal.psd",
        "snake16_psb_mode7_cmyk16_missing_composite_multilayer_normal_high_offset_xxxlarge.psd",
        padding_bytes=0x800000,
    )
    mutate_psb_high_offset_valid(
        "snake16_psb_mode7_cmyk16_missing_composite_multilayer_normal.psd",
        "snake16_psb_mode7_cmyk16_missing_composite_multilayer_normal_high_offset_xxxxlarge.psd",
        padding_bytes=0x1000000,
    )
    mutate_psb_high_offset_valid(
        "snake16_psb_mode7_cmyk16_missing_composite_multilayer_normal.psd",
        "snake16_psb_mode7_cmyk16_missing_composite_multilayer_normal_high_offset_xxxxxlarge.psd",
        padding_bytes=0x2000000,
    )
    mutate_psb_high_offset_valid(
        "snake16_psb_cmyk16_missing_composite_multilayer_normal_rle.psd",
        "snake16_psb_cmyk16_missing_composite_multilayer_normal_rle_high_offset.psd",
    )
    mutate_psb_high_offset_valid(
        "snake16_psb_mode7_cmyk16_missing_composite_multilayer_normal_rle.psd",
        "snake16_psb_mode7_cmyk16_missing_composite_multilayer_normal_rle_high_offset.psd",
    )
    mutate_psb_high_offset_valid(
        "snake16_psb_cmyk16_missing_composite_multilayer_normal_rle.psd",
        "snake16_psb_cmyk16_missing_composite_multilayer_normal_rle_high_offset_large.psd",
        padding_bytes=0x100000,
    )
    mutate_psb_high_offset_valid(
        "snake16_psb_mode7_cmyk16_missing_composite_multilayer_normal_rle.psd",
        "snake16_psb_mode7_cmyk16_missing_composite_multilayer_normal_rle_high_offset_large.psd",
        padding_bytes=0x100000,
    )
    mutate_psb_high_offset_valid(
        "snake16_psb_cmyk16_missing_composite_multilayer_normal_rle.psd",
        "snake16_psb_cmyk16_missing_composite_multilayer_normal_rle_high_offset_xlarge.psd",
        padding_bytes=0x200000,
    )
    mutate_psb_high_offset_valid(
        "snake16_psb_mode7_cmyk16_missing_composite_multilayer_normal_rle.psd",
        "snake16_psb_mode7_cmyk16_missing_composite_multilayer_normal_rle_high_offset_xlarge.psd",
        padding_bytes=0x200000,
    )
    mutate_psb_high_offset_valid(
        "snake16_psb_cmyk16_missing_composite_multilayer_normal_rle.psd",
        "snake16_psb_cmyk16_missing_composite_multilayer_normal_rle_high_offset_xxlarge.psd",
        padding_bytes=0x400000,
    )
    mutate_psb_high_offset_valid(
        "snake16_psb_mode7_cmyk16_missing_composite_multilayer_normal_rle.psd",
        "snake16_psb_mode7_cmyk16_missing_composite_multilayer_normal_rle_high_offset_xxlarge.psd",
        padding_bytes=0x400000,
    )
    mutate_psb_high_offset_valid(
        "snake16_psb_cmyk16_missing_composite_multilayer_normal_rle.psd",
        "snake16_psb_cmyk16_missing_composite_multilayer_normal_rle_high_offset_xxxlarge.psd",
        padding_bytes=0x800000,
    )
    mutate_psb_high_offset_valid(
        "snake16_psb_cmyk16_missing_composite_multilayer_normal_rle.psd",
        "snake16_psb_cmyk16_missing_composite_multilayer_normal_rle_high_offset_xxxxlarge.psd",
        padding_bytes=0x1000000,
    )
    mutate_psb_high_offset_valid(
        "snake16_psb_cmyk16_missing_composite_multilayer_normal_rle.psd",
        "snake16_psb_cmyk16_missing_composite_multilayer_normal_rle_high_offset_xxxxxlarge.psd",
        padding_bytes=0x2000000,
    )
    mutate_psb_high_offset_valid(
        "snake16_psb_mode7_cmyk16_missing_composite_multilayer_normal_rle.psd",
        "snake16_psb_mode7_cmyk16_missing_composite_multilayer_normal_rle_high_offset_xxxlarge.psd",
        padding_bytes=0x800000,
    )
    mutate_psb_high_offset_valid(
        "snake16_psb_mode7_cmyk16_missing_composite_multilayer_normal_rle.psd",
        "snake16_psb_mode7_cmyk16_missing_composite_multilayer_normal_rle_high_offset_xxxxlarge.psd",
        padding_bytes=0x1000000,
    )
    mutate_psb_high_offset_valid(
        "snake16_psb_mode7_cmyk16_missing_composite_multilayer_normal_rle.psd",
        "snake16_psb_mode7_cmyk16_missing_composite_multilayer_normal_rle_high_offset_xxxxxlarge.psd",
        padding_bytes=0x2000000,
    )
    mutate_psb_layer_info_end_overrun(
        "snake16_psb_cmyk16_missing_composite_multilayer_normal_high_offset.psd",
        "snake16_psb_cmyk16_missing_composite_multilayer_normal_high_offset_layer_info_end_overrun.psd",
    )
    mutate_psb_layer_info_end_overrun(
        "snake16_psb_mode7_cmyk16_missing_composite_multilayer_normal_high_offset.psd",
        "snake16_psb_mode7_cmyk16_missing_composite_multilayer_normal_high_offset_layer_info_end_overrun.psd",
    )
    mutate_psb_layer_info_end_overrun(
        "snake16_psb_cmyk16_missing_composite_multilayer_normal_high_offset_xxxlarge.psd",
        "snake16_psb_cmyk16_missing_composite_multilayer_normal_high_offset_xxxlarge_layer_info_end_overrun.psd",
    )
    mutate_psb_layer_info_end_overrun(
        "snake16_psb_cmyk16_missing_composite_multilayer_normal_high_offset_xxxxlarge.psd",
        "snake16_psb_cmyk16_missing_composite_multilayer_normal_high_offset_xxxxlarge_layer_info_end_overrun.psd",
    )
    mutate_psb_layer_info_end_overrun(
        "snake16_psb_cmyk16_missing_composite_multilayer_normal_high_offset_xxxxxlarge.psd",
        "snake16_psb_cmyk16_missing_composite_multilayer_normal_high_offset_xxxxxlarge_layer_info_end_overrun.psd",
    )
    mutate_psb_layer_info_end_overrun(
        "snake16_psb_mode7_cmyk16_missing_composite_multilayer_normal_high_offset_xxxlarge.psd",
        "snake16_psb_mode7_cmyk16_missing_composite_multilayer_normal_high_offset_xxxlarge_layer_info_end_overrun.psd",
    )
    mutate_psb_layer_info_end_overrun(
        "snake16_psb_mode7_cmyk16_missing_composite_multilayer_normal_high_offset_xxxxlarge.psd",
        "snake16_psb_mode7_cmyk16_missing_composite_multilayer_normal_high_offset_xxxxlarge_layer_info_end_overrun.psd",
    )
    mutate_psb_layer_info_end_overrun(
        "snake16_psb_mode7_cmyk16_missing_composite_multilayer_normal_high_offset_xxxxxlarge.psd",
        "snake16_psb_mode7_cmyk16_missing_composite_multilayer_normal_high_offset_xxxxxlarge_layer_info_end_overrun.psd",
    )
    mutate_psb_channel_window_overrun(
        "snake16_psb_cmyk16_missing_composite_multilayer_normal_high_offset.psd",
        "snake16_psb_cmyk16_missing_composite_multilayer_normal_high_offset_channel_window_overrun.psd",
    )
    mutate_psb_channel_window_overrun(
        "snake16_psb_mode7_cmyk16_missing_composite_multilayer_normal_high_offset.psd",
        "snake16_psb_mode7_cmyk16_missing_composite_multilayer_normal_high_offset_channel_window_overrun.psd",
    )
    mutate_psb_channel_window_overrun(
        "snake16_psb_cmyk16_missing_composite_multilayer_normal_high_offset_large.psd",
        "snake16_psb_cmyk16_missing_composite_multilayer_normal_high_offset_large_channel_window_overrun.psd",
    )
    mutate_psb_channel_window_overrun(
        "snake16_psb_mode7_cmyk16_missing_composite_multilayer_normal_high_offset_large.psd",
        "snake16_psb_mode7_cmyk16_missing_composite_multilayer_normal_high_offset_large_channel_window_overrun.psd",
    )
    mutate_psb_channel_window_overrun(
        "snake16_psb_cmyk16_missing_composite_multilayer_normal_high_offset_xlarge.psd",
        "snake16_psb_cmyk16_missing_composite_multilayer_normal_high_offset_xlarge_channel_window_overrun.psd",
    )
    mutate_psb_channel_window_overrun(
        "snake16_psb_mode7_cmyk16_missing_composite_multilayer_normal_high_offset_xlarge.psd",
        "snake16_psb_mode7_cmyk16_missing_composite_multilayer_normal_high_offset_xlarge_channel_window_overrun.psd",
    )
    mutate_psb_channel_window_overrun(
        "snake16_psb_cmyk16_missing_composite_multilayer_normal_high_offset_xxlarge.psd",
        "snake16_psb_cmyk16_missing_composite_multilayer_normal_high_offset_xxlarge_channel_window_overrun.psd",
    )
    mutate_psb_channel_window_overrun(
        "snake16_psb_mode7_cmyk16_missing_composite_multilayer_normal_high_offset_xxlarge.psd",
        "snake16_psb_mode7_cmyk16_missing_composite_multilayer_normal_high_offset_xxlarge_channel_window_overrun.psd",
    )
    mutate_psb_channel_window_overrun(
        "snake16_psb_cmyk16_missing_composite_multilayer_normal_high_offset_xxxlarge.psd",
        "snake16_psb_cmyk16_missing_composite_multilayer_normal_high_offset_xxxlarge_channel_window_overrun.psd",
    )
    mutate_psb_channel_window_overrun(
        "snake16_psb_cmyk16_missing_composite_multilayer_normal_high_offset_xxxxlarge.psd",
        "snake16_psb_cmyk16_missing_composite_multilayer_normal_high_offset_xxxxlarge_channel_window_overrun.psd",
    )
    mutate_psb_channel_window_overrun(
        "snake16_psb_cmyk16_missing_composite_multilayer_normal_high_offset_xxxxxlarge.psd",
        "snake16_psb_cmyk16_missing_composite_multilayer_normal_high_offset_xxxxxlarge_channel_window_overrun.psd",
    )
    mutate_psb_channel_window_overrun(
        "snake16_psb_mode7_cmyk16_missing_composite_multilayer_normal_high_offset_xxxlarge.psd",
        "snake16_psb_mode7_cmyk16_missing_composite_multilayer_normal_high_offset_xxxlarge_channel_window_overrun.psd",
    )
    mutate_psb_channel_window_overrun(
        "snake16_psb_mode7_cmyk16_missing_composite_multilayer_normal_high_offset_xxxxlarge.psd",
        "snake16_psb_mode7_cmyk16_missing_composite_multilayer_normal_high_offset_xxxxlarge_channel_window_overrun.psd",
    )
    mutate_psb_channel_window_overrun(
        "snake16_psb_mode7_cmyk16_missing_composite_multilayer_normal_high_offset_xxxxxlarge.psd",
        "snake16_psb_mode7_cmyk16_missing_composite_multilayer_normal_high_offset_xxxxxlarge_channel_window_overrun.psd",
    )
    mutate_psb_rle_payload_window_overrun(
        "snake16_psb_cmyk16_missing_composite_multilayer_normal_rle_high_offset.psd",
        "snake16_psb_cmyk16_missing_composite_multilayer_normal_rle_high_offset_rle_payload_window_overrun.psd",
    )
    mutate_psb_rle_payload_window_overrun(
        "snake16_psb_mode7_cmyk16_missing_composite_multilayer_normal_rle_high_offset.psd",
        "snake16_psb_mode7_cmyk16_missing_composite_multilayer_normal_rle_high_offset_rle_payload_window_overrun.psd",
    )
    mutate_psb_rle_payload_window_overrun(
        "snake16_psb_cmyk16_missing_composite_multilayer_normal_rle_high_offset_large.psd",
        "snake16_psb_cmyk16_missing_composite_multilayer_normal_rle_high_offset_large_rle_payload_window_overrun.psd",
    )
    mutate_psb_rle_payload_window_overrun(
        "snake16_psb_mode7_cmyk16_missing_composite_multilayer_normal_rle_high_offset_large.psd",
        "snake16_psb_mode7_cmyk16_missing_composite_multilayer_normal_rle_high_offset_large_rle_payload_window_overrun.psd",
    )
    mutate_psb_rle_payload_window_overrun(
        "snake16_psb_cmyk16_missing_composite_multilayer_normal_rle_high_offset_xlarge.psd",
        "snake16_psb_cmyk16_missing_composite_multilayer_normal_rle_high_offset_xlarge_rle_payload_window_overrun.psd",
    )
    mutate_psb_rle_payload_window_overrun(
        "snake16_psb_mode7_cmyk16_missing_composite_multilayer_normal_rle_high_offset_xlarge.psd",
        "snake16_psb_mode7_cmyk16_missing_composite_multilayer_normal_rle_high_offset_xlarge_rle_payload_window_overrun.psd",
    )
    mutate_psb_rle_payload_window_overrun(
        "snake16_psb_cmyk16_missing_composite_multilayer_normal_rle_high_offset_xxlarge.psd",
        "snake16_psb_cmyk16_missing_composite_multilayer_normal_rle_high_offset_xxlarge_rle_payload_window_overrun.psd",
    )
    mutate_psb_rle_payload_window_overrun(
        "snake16_psb_mode7_cmyk16_missing_composite_multilayer_normal_rle_high_offset_xxlarge.psd",
        "snake16_psb_mode7_cmyk16_missing_composite_multilayer_normal_rle_high_offset_xxlarge_rle_payload_window_overrun.psd",
    )
    mutate_psb_rle_payload_window_overrun(
        "snake16_psb_cmyk16_missing_composite_multilayer_normal_rle_high_offset_xxxlarge.psd",
        "snake16_psb_cmyk16_missing_composite_multilayer_normal_rle_high_offset_xxxlarge_rle_payload_window_overrun.psd",
    )
    mutate_psb_rle_payload_window_overrun(
        "snake16_psb_cmyk16_missing_composite_multilayer_normal_rle_high_offset_xxxxlarge.psd",
        "snake16_psb_cmyk16_missing_composite_multilayer_normal_rle_high_offset_xxxxlarge_rle_payload_window_overrun.psd",
    )
    mutate_psb_rle_payload_window_overrun(
        "snake16_psb_cmyk16_missing_composite_multilayer_normal_rle_high_offset_xxxxxlarge.psd",
        "snake16_psb_cmyk16_missing_composite_multilayer_normal_rle_high_offset_xxxxxlarge_rle_payload_window_overrun.psd",
    )
    mutate_psb_rle_payload_window_overrun(
        "snake16_psb_mode7_cmyk16_missing_composite_multilayer_normal_rle_high_offset_xxxlarge.psd",
        "snake16_psb_mode7_cmyk16_missing_composite_multilayer_normal_rle_high_offset_xxxlarge_rle_payload_window_overrun.psd",
    )
    mutate_psb_rle_payload_window_overrun(
        "snake16_psb_mode7_cmyk16_missing_composite_multilayer_normal_rle_high_offset_xxxxlarge.psd",
        "snake16_psb_mode7_cmyk16_missing_composite_multilayer_normal_rle_high_offset_xxxxlarge_rle_payload_window_overrun.psd",
    )
    mutate_psb_rle_payload_window_overrun(
        "snake16_psb_mode7_cmyk16_missing_composite_multilayer_normal_rle_high_offset_xxxxxlarge.psd",
        "snake16_psb_mode7_cmyk16_missing_composite_multilayer_normal_rle_high_offset_xxxxxlarge_rle_payload_window_overrun.psd",
    )
    mutate_psb_high_offset_valid(
        "snake16_psb_cmyk32_missing_composite_multilayer_normal.psd",
        "snake16_psb_cmyk32_missing_composite_multilayer_normal_high_offset.psd",
    )
    mutate_psb_high_offset_valid(
        "snake16_psb_mode7_cmyk32_missing_composite_multilayer_normal.psd",
        "snake16_psb_mode7_cmyk32_missing_composite_multilayer_normal_high_offset.psd",
    )
    mutate_psb_high_offset_valid(
        "snake16_psb_cmyk32_missing_composite_multilayer_normal.psd",
        "snake16_psb_cmyk32_missing_composite_multilayer_normal_high_offset_large.psd",
        padding_bytes=0x100000,
    )
    mutate_psb_high_offset_valid(
        "snake16_psb_mode7_cmyk32_missing_composite_multilayer_normal.psd",
        "snake16_psb_mode7_cmyk32_missing_composite_multilayer_normal_high_offset_large.psd",
        padding_bytes=0x100000,
    )
    mutate_psb_high_offset_valid(
        "snake16_psb_cmyk32_missing_composite_multilayer_normal.psd",
        "snake16_psb_cmyk32_missing_composite_multilayer_normal_high_offset_xlarge.psd",
        padding_bytes=0x200000,
    )
    mutate_psb_high_offset_valid(
        "snake16_psb_mode7_cmyk32_missing_composite_multilayer_normal.psd",
        "snake16_psb_mode7_cmyk32_missing_composite_multilayer_normal_high_offset_xlarge.psd",
        padding_bytes=0x200000,
    )
    mutate_psb_high_offset_valid(
        "snake16_psb_cmyk32_missing_composite_multilayer_normal.psd",
        "snake16_psb_cmyk32_missing_composite_multilayer_normal_high_offset_xxlarge.psd",
        padding_bytes=0x400000,
    )
    mutate_psb_high_offset_valid(
        "snake16_psb_mode7_cmyk32_missing_composite_multilayer_normal.psd",
        "snake16_psb_mode7_cmyk32_missing_composite_multilayer_normal_high_offset_xxlarge.psd",
        padding_bytes=0x400000,
    )
    mutate_psb_high_offset_valid(
        "snake16_psb_cmyk32_missing_composite_multilayer_normal.psd",
        "snake16_psb_cmyk32_missing_composite_multilayer_normal_high_offset_xxxlarge.psd",
        padding_bytes=0x800000,
    )
    mutate_psb_high_offset_valid(
        "snake16_psb_cmyk32_missing_composite_multilayer_normal.psd",
        "snake16_psb_cmyk32_missing_composite_multilayer_normal_high_offset_xxxxlarge.psd",
        padding_bytes=0x1000000,
    )
    mutate_psb_high_offset_valid(
        "snake16_psb_cmyk32_missing_composite_multilayer_normal.psd",
        "snake16_psb_cmyk32_missing_composite_multilayer_normal_high_offset_xxxxxlarge.psd",
        padding_bytes=0x2000000,
    )
    mutate_psb_high_offset_valid(
        "snake16_psb_mode7_cmyk32_missing_composite_multilayer_normal.psd",
        "snake16_psb_mode7_cmyk32_missing_composite_multilayer_normal_high_offset_xxxlarge.psd",
        padding_bytes=0x800000,
    )
    mutate_psb_high_offset_valid(
        "snake16_psb_mode7_cmyk32_missing_composite_multilayer_normal.psd",
        "snake16_psb_mode7_cmyk32_missing_composite_multilayer_normal_high_offset_xxxxlarge.psd",
        padding_bytes=0x1000000,
    )
    mutate_psb_high_offset_valid(
        "snake16_psb_mode7_cmyk32_missing_composite_multilayer_normal.psd",
        "snake16_psb_mode7_cmyk32_missing_composite_multilayer_normal_high_offset_xxxxxlarge.psd",
        padding_bytes=0x2000000,
    )
    mutate_psb_high_offset_valid(
        "snake16_psb_cmyk32_missing_composite_multilayer_normal_rle.psd",
        "snake16_psb_cmyk32_missing_composite_multilayer_normal_rle_high_offset.psd",
    )
    mutate_psb_high_offset_valid(
        "snake16_psb_mode7_cmyk32_missing_composite_multilayer_normal_rle.psd",
        "snake16_psb_mode7_cmyk32_missing_composite_multilayer_normal_rle_high_offset.psd",
    )
    mutate_psb_high_offset_valid(
        "snake16_psb_cmyk32_missing_composite_multilayer_normal_rle.psd",
        "snake16_psb_cmyk32_missing_composite_multilayer_normal_rle_high_offset_large.psd",
        padding_bytes=0x100000,
    )
    mutate_psb_high_offset_valid(
        "snake16_psb_mode7_cmyk32_missing_composite_multilayer_normal_rle.psd",
        "snake16_psb_mode7_cmyk32_missing_composite_multilayer_normal_rle_high_offset_large.psd",
        padding_bytes=0x100000,
    )
    mutate_psb_high_offset_valid(
        "snake16_psb_cmyk32_missing_composite_multilayer_normal_rle.psd",
        "snake16_psb_cmyk32_missing_composite_multilayer_normal_rle_high_offset_xlarge.psd",
        padding_bytes=0x200000,
    )
    mutate_psb_high_offset_valid(
        "snake16_psb_mode7_cmyk32_missing_composite_multilayer_normal_rle.psd",
        "snake16_psb_mode7_cmyk32_missing_composite_multilayer_normal_rle_high_offset_xlarge.psd",
        padding_bytes=0x200000,
    )
    mutate_psb_high_offset_valid(
        "snake16_psb_cmyk32_missing_composite_multilayer_normal_rle.psd",
        "snake16_psb_cmyk32_missing_composite_multilayer_normal_rle_high_offset_xxlarge.psd",
        padding_bytes=0x400000,
    )
    mutate_psb_high_offset_valid(
        "snake16_psb_mode7_cmyk32_missing_composite_multilayer_normal_rle.psd",
        "snake16_psb_mode7_cmyk32_missing_composite_multilayer_normal_rle_high_offset_xxlarge.psd",
        padding_bytes=0x400000,
    )
    mutate_psb_high_offset_valid(
        "snake16_psb_cmyk32_missing_composite_multilayer_normal_rle.psd",
        "snake16_psb_cmyk32_missing_composite_multilayer_normal_rle_high_offset_xxxlarge.psd",
        padding_bytes=0x800000,
    )
    mutate_psb_high_offset_valid(
        "snake16_psb_cmyk32_missing_composite_multilayer_normal_rle.psd",
        "snake16_psb_cmyk32_missing_composite_multilayer_normal_rle_high_offset_xxxxlarge.psd",
        padding_bytes=0x1000000,
    )
    mutate_psb_high_offset_valid(
        "snake16_psb_cmyk32_missing_composite_multilayer_normal_rle.psd",
        "snake16_psb_cmyk32_missing_composite_multilayer_normal_rle_high_offset_xxxxxlarge.psd",
        padding_bytes=0x2000000,
    )
    mutate_psb_high_offset_valid(
        "snake16_psb_mode7_cmyk32_missing_composite_multilayer_normal_rle.psd",
        "snake16_psb_mode7_cmyk32_missing_composite_multilayer_normal_rle_high_offset_xxxlarge.psd",
        padding_bytes=0x800000,
    )
    mutate_psb_high_offset_valid(
        "snake16_psb_mode7_cmyk32_missing_composite_multilayer_normal_rle.psd",
        "snake16_psb_mode7_cmyk32_missing_composite_multilayer_normal_rle_high_offset_xxxxlarge.psd",
        padding_bytes=0x1000000,
    )
    mutate_psb_high_offset_valid(
        "snake16_psb_mode7_cmyk32_missing_composite_multilayer_normal_rle.psd",
        "snake16_psb_mode7_cmyk32_missing_composite_multilayer_normal_rle_high_offset_xxxxxlarge.psd",
        padding_bytes=0x2000000,
    )
    mutate_psb_layer_info_end_overrun(
        "snake16_psb_cmyk32_missing_composite_multilayer_normal_high_offset.psd",
        "snake16_psb_cmyk32_missing_composite_multilayer_normal_high_offset_layer_info_end_overrun.psd",
    )
    mutate_psb_layer_info_end_overrun(
        "snake16_psb_mode7_cmyk32_missing_composite_multilayer_normal_high_offset.psd",
        "snake16_psb_mode7_cmyk32_missing_composite_multilayer_normal_high_offset_layer_info_end_overrun.psd",
    )
    mutate_psb_layer_info_end_overrun(
        "snake16_psb_cmyk32_missing_composite_multilayer_normal_high_offset_large.psd",
        "snake16_psb_cmyk32_missing_composite_multilayer_normal_high_offset_large_layer_info_end_overrun.psd",
    )
    mutate_psb_layer_info_end_overrun(
        "snake16_psb_mode7_cmyk32_missing_composite_multilayer_normal_high_offset_large.psd",
        "snake16_psb_mode7_cmyk32_missing_composite_multilayer_normal_high_offset_large_layer_info_end_overrun.psd",
    )
    mutate_psb_layer_info_end_overrun(
        "snake16_psb_cmyk32_missing_composite_multilayer_normal_high_offset_xlarge.psd",
        "snake16_psb_cmyk32_missing_composite_multilayer_normal_high_offset_xlarge_layer_info_end_overrun.psd",
    )
    mutate_psb_layer_info_end_overrun(
        "snake16_psb_mode7_cmyk32_missing_composite_multilayer_normal_high_offset_xlarge.psd",
        "snake16_psb_mode7_cmyk32_missing_composite_multilayer_normal_high_offset_xlarge_layer_info_end_overrun.psd",
    )
    mutate_psb_layer_info_end_overrun(
        "snake16_psb_cmyk32_missing_composite_multilayer_normal_high_offset_xxlarge.psd",
        "snake16_psb_cmyk32_missing_composite_multilayer_normal_high_offset_xxlarge_layer_info_end_overrun.psd",
    )
    mutate_psb_layer_info_end_overrun(
        "snake16_psb_mode7_cmyk32_missing_composite_multilayer_normal_high_offset_xxlarge.psd",
        "snake16_psb_mode7_cmyk32_missing_composite_multilayer_normal_high_offset_xxlarge_layer_info_end_overrun.psd",
    )
    mutate_psb_layer_info_end_overrun(
        "snake16_psb_cmyk32_missing_composite_multilayer_normal_high_offset_xxxlarge.psd",
        "snake16_psb_cmyk32_missing_composite_multilayer_normal_high_offset_xxxlarge_layer_info_end_overrun.psd",
    )
    mutate_psb_layer_info_end_overrun(
        "snake16_psb_cmyk32_missing_composite_multilayer_normal_high_offset_xxxxlarge.psd",
        "snake16_psb_cmyk32_missing_composite_multilayer_normal_high_offset_xxxxlarge_layer_info_end_overrun.psd",
    )
    mutate_psb_layer_info_end_overrun(
        "snake16_psb_cmyk32_missing_composite_multilayer_normal_high_offset_xxxxxlarge.psd",
        "snake16_psb_cmyk32_missing_composite_multilayer_normal_high_offset_xxxxxlarge_layer_info_end_overrun.psd",
    )
    mutate_psb_layer_info_end_overrun(
        "snake16_psb_mode7_cmyk32_missing_composite_multilayer_normal_high_offset_xxxlarge.psd",
        "snake16_psb_mode7_cmyk32_missing_composite_multilayer_normal_high_offset_xxxlarge_layer_info_end_overrun.psd",
    )
    mutate_psb_layer_info_end_overrun(
        "snake16_psb_mode7_cmyk32_missing_composite_multilayer_normal_high_offset_xxxxlarge.psd",
        "snake16_psb_mode7_cmyk32_missing_composite_multilayer_normal_high_offset_xxxxlarge_layer_info_end_overrun.psd",
    )
    mutate_psb_layer_info_end_overrun(
        "snake16_psb_mode7_cmyk32_missing_composite_multilayer_normal_high_offset_xxxxxlarge.psd",
        "snake16_psb_mode7_cmyk32_missing_composite_multilayer_normal_high_offset_xxxxxlarge_layer_info_end_overrun.psd",
    )
    mutate_psb_channel_window_overrun(
        "snake16_psb_cmyk32_missing_composite_multilayer_normal_high_offset.psd",
        "snake16_psb_cmyk32_missing_composite_multilayer_normal_high_offset_channel_window_overrun.psd",
    )
    mutate_psb_channel_window_overrun(
        "snake16_psb_mode7_cmyk32_missing_composite_multilayer_normal_high_offset.psd",
        "snake16_psb_mode7_cmyk32_missing_composite_multilayer_normal_high_offset_channel_window_overrun.psd",
    )
    mutate_psb_channel_window_overrun(
        "snake16_psb_cmyk32_missing_composite_multilayer_normal_high_offset_large.psd",
        "snake16_psb_cmyk32_missing_composite_multilayer_normal_high_offset_large_channel_window_overrun.psd",
    )
    mutate_psb_channel_window_overrun(
        "snake16_psb_mode7_cmyk32_missing_composite_multilayer_normal_high_offset_large.psd",
        "snake16_psb_mode7_cmyk32_missing_composite_multilayer_normal_high_offset_large_channel_window_overrun.psd",
    )
    mutate_psb_channel_window_overrun(
        "snake16_psb_cmyk32_missing_composite_multilayer_normal_high_offset_xlarge.psd",
        "snake16_psb_cmyk32_missing_composite_multilayer_normal_high_offset_xlarge_channel_window_overrun.psd",
    )
    mutate_psb_channel_window_overrun(
        "snake16_psb_mode7_cmyk32_missing_composite_multilayer_normal_high_offset_xlarge.psd",
        "snake16_psb_mode7_cmyk32_missing_composite_multilayer_normal_high_offset_xlarge_channel_window_overrun.psd",
    )
    mutate_psb_channel_window_overrun(
        "snake16_psb_cmyk32_missing_composite_multilayer_normal_high_offset_xxlarge.psd",
        "snake16_psb_cmyk32_missing_composite_multilayer_normal_high_offset_xxlarge_channel_window_overrun.psd",
    )
    mutate_psb_channel_window_overrun(
        "snake16_psb_mode7_cmyk32_missing_composite_multilayer_normal_high_offset_xxlarge.psd",
        "snake16_psb_mode7_cmyk32_missing_composite_multilayer_normal_high_offset_xxlarge_channel_window_overrun.psd",
    )
    mutate_psb_channel_window_overrun(
        "snake16_psb_cmyk32_missing_composite_multilayer_normal_high_offset_xxxlarge.psd",
        "snake16_psb_cmyk32_missing_composite_multilayer_normal_high_offset_xxxlarge_channel_window_overrun.psd",
    )
    mutate_psb_channel_window_overrun(
        "snake16_psb_cmyk32_missing_composite_multilayer_normal_high_offset_xxxxlarge.psd",
        "snake16_psb_cmyk32_missing_composite_multilayer_normal_high_offset_xxxxlarge_channel_window_overrun.psd",
    )
    mutate_psb_channel_window_overrun(
        "snake16_psb_cmyk32_missing_composite_multilayer_normal_high_offset_xxxxxlarge.psd",
        "snake16_psb_cmyk32_missing_composite_multilayer_normal_high_offset_xxxxxlarge_channel_window_overrun.psd",
    )
    mutate_psb_channel_window_overrun(
        "snake16_psb_mode7_cmyk32_missing_composite_multilayer_normal_high_offset_xxxlarge.psd",
        "snake16_psb_mode7_cmyk32_missing_composite_multilayer_normal_high_offset_xxxlarge_channel_window_overrun.psd",
    )
    mutate_psb_channel_window_overrun(
        "snake16_psb_mode7_cmyk32_missing_composite_multilayer_normal_high_offset_xxxxlarge.psd",
        "snake16_psb_mode7_cmyk32_missing_composite_multilayer_normal_high_offset_xxxxlarge_channel_window_overrun.psd",
    )
    mutate_psb_channel_window_overrun(
        "snake16_psb_mode7_cmyk32_missing_composite_multilayer_normal_high_offset_xxxxxlarge.psd",
        "snake16_psb_mode7_cmyk32_missing_composite_multilayer_normal_high_offset_xxxxxlarge_channel_window_overrun.psd",
    )
    mutate_psb_rle_payload_window_overrun(
        "snake16_psb_cmyk32_missing_composite_multilayer_normal_rle_high_offset.psd",
        "snake16_psb_cmyk32_missing_composite_multilayer_normal_rle_high_offset_rle_payload_window_overrun.psd",
    )
    mutate_psb_rle_payload_window_overrun(
        "snake16_psb_mode7_cmyk32_missing_composite_multilayer_normal_rle_high_offset.psd",
        "snake16_psb_mode7_cmyk32_missing_composite_multilayer_normal_rle_high_offset_rle_payload_window_overrun.psd",
    )
    mutate_psb_rle_payload_window_overrun(
        "snake16_psb_cmyk32_missing_composite_multilayer_normal_rle_high_offset_large.psd",
        "snake16_psb_cmyk32_missing_composite_multilayer_normal_rle_high_offset_large_rle_payload_window_overrun.psd",
    )
    mutate_psb_rle_payload_window_overrun(
        "snake16_psb_mode7_cmyk32_missing_composite_multilayer_normal_rle_high_offset_large.psd",
        "snake16_psb_mode7_cmyk32_missing_composite_multilayer_normal_rle_high_offset_large_rle_payload_window_overrun.psd",
    )
    mutate_psb_rle_payload_window_overrun(
        "snake16_psb_cmyk32_missing_composite_multilayer_normal_rle_high_offset_xlarge.psd",
        "snake16_psb_cmyk32_missing_composite_multilayer_normal_rle_high_offset_xlarge_rle_payload_window_overrun.psd",
    )
    mutate_psb_rle_payload_window_overrun(
        "snake16_psb_mode7_cmyk32_missing_composite_multilayer_normal_rle_high_offset_xlarge.psd",
        "snake16_psb_mode7_cmyk32_missing_composite_multilayer_normal_rle_high_offset_xlarge_rle_payload_window_overrun.psd",
    )
    mutate_psb_rle_payload_window_overrun(
        "snake16_psb_cmyk32_missing_composite_multilayer_normal_rle_high_offset_xxlarge.psd",
        "snake16_psb_cmyk32_missing_composite_multilayer_normal_rle_high_offset_xxlarge_rle_payload_window_overrun.psd",
    )
    mutate_psb_rle_payload_window_overrun(
        "snake16_psb_mode7_cmyk32_missing_composite_multilayer_normal_rle_high_offset_xxlarge.psd",
        "snake16_psb_mode7_cmyk32_missing_composite_multilayer_normal_rle_high_offset_xxlarge_rle_payload_window_overrun.psd",
    )
    mutate_psb_rle_payload_window_overrun(
        "snake16_psb_cmyk32_missing_composite_multilayer_normal_rle_high_offset_xxxlarge.psd",
        "snake16_psb_cmyk32_missing_composite_multilayer_normal_rle_high_offset_xxxlarge_rle_payload_window_overrun.psd",
    )
    mutate_psb_rle_payload_window_overrun(
        "snake16_psb_cmyk32_missing_composite_multilayer_normal_rle_high_offset_xxxxlarge.psd",
        "snake16_psb_cmyk32_missing_composite_multilayer_normal_rle_high_offset_xxxxlarge_rle_payload_window_overrun.psd",
    )
    mutate_psb_rle_payload_window_overrun(
        "snake16_psb_cmyk32_missing_composite_multilayer_normal_rle_high_offset_xxxxxlarge.psd",
        "snake16_psb_cmyk32_missing_composite_multilayer_normal_rle_high_offset_xxxxxlarge_rle_payload_window_overrun.psd",
    )
    mutate_psb_rle_payload_window_overrun(
        "snake16_psb_mode7_cmyk32_missing_composite_multilayer_normal_rle_high_offset_xxxlarge.psd",
        "snake16_psb_mode7_cmyk32_missing_composite_multilayer_normal_rle_high_offset_xxxlarge_rle_payload_window_overrun.psd",
    )
    mutate_psb_rle_payload_window_overrun(
        "snake16_psb_mode7_cmyk32_missing_composite_multilayer_normal_rle_high_offset_xxxxlarge.psd",
        "snake16_psb_mode7_cmyk32_missing_composite_multilayer_normal_rle_high_offset_xxxxlarge_rle_payload_window_overrun.psd",
    )
    mutate_psb_rle_payload_window_overrun(
        "snake16_psb_mode7_cmyk32_missing_composite_multilayer_normal_rle_high_offset_xxxxxlarge.psd",
        "snake16_psb_mode7_cmyk32_missing_composite_multilayer_normal_rle_high_offset_xxxxxlarge_rle_payload_window_overrun.psd",
    )
    mutate_psb_high_offset_valid(
        "snake16_psb_cmyk16_missing_composite_multilayer_normal.psd",
        "snake16_psb_cmyk16_missing_composite_multilayer_normal_high_offset_xxxxxxlarge.psd",
        padding_bytes=0x4000000,
    )
    mutate_psb_high_offset_valid(
        "snake16_psb_mode7_cmyk16_missing_composite_multilayer_normal.psd",
        "snake16_psb_mode7_cmyk16_missing_composite_multilayer_normal_high_offset_xxxxxxlarge.psd",
        padding_bytes=0x4000000,
    )
    mutate_psb_high_offset_valid(
        "snake16_psb_cmyk16_missing_composite_multilayer_normal_rle.psd",
        "snake16_psb_cmyk16_missing_composite_multilayer_normal_rle_high_offset_xxxxxxlarge.psd",
        padding_bytes=0x4000000,
    )
    mutate_psb_high_offset_valid(
        "snake16_psb_mode7_cmyk16_missing_composite_multilayer_normal_rle.psd",
        "snake16_psb_mode7_cmyk16_missing_composite_multilayer_normal_rle_high_offset_xxxxxxlarge.psd",
        padding_bytes=0x4000000,
    )
    mutate_psb_high_offset_valid(
        "snake16_psb_cmyk32_missing_composite_multilayer_normal.psd",
        "snake16_psb_cmyk32_missing_composite_multilayer_normal_high_offset_xxxxxxlarge.psd",
        padding_bytes=0x4000000,
    )
    mutate_psb_high_offset_valid(
        "snake16_psb_mode7_cmyk32_missing_composite_multilayer_normal.psd",
        "snake16_psb_mode7_cmyk32_missing_composite_multilayer_normal_high_offset_xxxxxxlarge.psd",
        padding_bytes=0x4000000,
    )
    mutate_psb_high_offset_valid(
        "snake16_psb_cmyk32_missing_composite_multilayer_normal_rle.psd",
        "snake16_psb_cmyk32_missing_composite_multilayer_normal_rle_high_offset_xxxxxxlarge.psd",
        padding_bytes=0x4000000,
    )
    mutate_psb_high_offset_valid(
        "snake16_psb_mode7_cmyk32_missing_composite_multilayer_normal_rle.psd",
        "snake16_psb_mode7_cmyk32_missing_composite_multilayer_normal_rle_high_offset_xxxxxxlarge.psd",
        padding_bytes=0x4000000,
    )
    mutate_psb_layer_info_end_overrun(
        "snake16_psb_cmyk16_missing_composite_multilayer_normal_high_offset_xxxxxxlarge.psd",
        "snake16_psb_cmyk16_missing_composite_multilayer_normal_high_offset_xxxxxxlarge_layer_info_end_overrun.psd",
    )
    mutate_psb_layer_info_end_overrun(
        "snake16_psb_mode7_cmyk16_missing_composite_multilayer_normal_high_offset_xxxxxxlarge.psd",
        "snake16_psb_mode7_cmyk16_missing_composite_multilayer_normal_high_offset_xxxxxxlarge_layer_info_end_overrun.psd",
    )
    mutate_psb_layer_info_end_overrun(
        "snake16_psb_cmyk32_missing_composite_multilayer_normal_high_offset_xxxxxxlarge.psd",
        "snake16_psb_cmyk32_missing_composite_multilayer_normal_high_offset_xxxxxxlarge_layer_info_end_overrun.psd",
    )
    mutate_psb_layer_info_end_overrun(
        "snake16_psb_mode7_cmyk32_missing_composite_multilayer_normal_high_offset_xxxxxxlarge.psd",
        "snake16_psb_mode7_cmyk32_missing_composite_multilayer_normal_high_offset_xxxxxxlarge_layer_info_end_overrun.psd",
    )
    mutate_psb_channel_window_overrun(
        "snake16_psb_cmyk16_missing_composite_multilayer_normal_high_offset_xxxxxxlarge.psd",
        "snake16_psb_cmyk16_missing_composite_multilayer_normal_high_offset_xxxxxxlarge_channel_window_overrun.psd",
    )
    mutate_psb_channel_window_overrun(
        "snake16_psb_mode7_cmyk16_missing_composite_multilayer_normal_high_offset_xxxxxxlarge.psd",
        "snake16_psb_mode7_cmyk16_missing_composite_multilayer_normal_high_offset_xxxxxxlarge_channel_window_overrun.psd",
    )
    mutate_psb_channel_window_overrun(
        "snake16_psb_cmyk32_missing_composite_multilayer_normal_high_offset_xxxxxxlarge.psd",
        "snake16_psb_cmyk32_missing_composite_multilayer_normal_high_offset_xxxxxxlarge_channel_window_overrun.psd",
    )
    mutate_psb_channel_window_overrun(
        "snake16_psb_mode7_cmyk32_missing_composite_multilayer_normal_high_offset_xxxxxxlarge.psd",
        "snake16_psb_mode7_cmyk32_missing_composite_multilayer_normal_high_offset_xxxxxxlarge_channel_window_overrun.psd",
    )
    mutate_psb_rle_payload_window_overrun(
        "snake16_psb_cmyk16_missing_composite_multilayer_normal_rle_high_offset_xxxxxxlarge.psd",
        "snake16_psb_cmyk16_missing_composite_multilayer_normal_rle_high_offset_xxxxxxlarge_rle_payload_window_overrun.psd",
    )
    mutate_psb_rle_payload_window_overrun(
        "snake16_psb_mode7_cmyk16_missing_composite_multilayer_normal_rle_high_offset_xxxxxxlarge.psd",
        "snake16_psb_mode7_cmyk16_missing_composite_multilayer_normal_rle_high_offset_xxxxxxlarge_rle_payload_window_overrun.psd",
    )
    mutate_psb_rle_payload_window_overrun(
        "snake16_psb_cmyk32_missing_composite_multilayer_normal_rle_high_offset_xxxxxxlarge.psd",
        "snake16_psb_cmyk32_missing_composite_multilayer_normal_rle_high_offset_xxxxxxlarge_rle_payload_window_overrun.psd",
    )
    mutate_psb_rle_payload_window_overrun(
        "snake16_psb_mode7_cmyk32_missing_composite_multilayer_normal_rle_high_offset_xxxxxxlarge.psd",
        "snake16_psb_mode7_cmyk32_missing_composite_multilayer_normal_rle_high_offset_xxxxxxlarge_rle_payload_window_overrun.psd",
    )
    # Keep generated files under the default 128 MiB allocator ceiling and
    # below the chunk grow boundary (next realloc to 256 MiB).
    # 0x8000000 (+source bytes) would exceed SIXEL_ALLOCATE_BYTES_MAX.
    mutate_psb_high_offset_valid(
        "snake16_psb_cmyk16_missing_composite_multilayer_normal.psd",
        "snake16_psb_cmyk16_missing_composite_multilayer_normal_high_offset_xxxxxxxlarge.psd",
        padding_bytes=0x7FF0000,
    )
    mutate_psb_high_offset_valid(
        "snake16_psb_mode7_cmyk16_missing_composite_multilayer_normal.psd",
        "snake16_psb_mode7_cmyk16_missing_composite_multilayer_normal_high_offset_xxxxxxxlarge.psd",
        padding_bytes=0x7FF0000,
    )
    mutate_psb_high_offset_valid(
        "snake16_psb_cmyk16_missing_composite_multilayer_normal_rle.psd",
        "snake16_psb_cmyk16_missing_composite_multilayer_normal_rle_high_offset_xxxxxxxlarge.psd",
        padding_bytes=0x7FF0000,
    )
    mutate_psb_high_offset_valid(
        "snake16_psb_mode7_cmyk16_missing_composite_multilayer_normal_rle.psd",
        "snake16_psb_mode7_cmyk16_missing_composite_multilayer_normal_rle_high_offset_xxxxxxxlarge.psd",
        padding_bytes=0x7FF0000,
    )
    mutate_psb_high_offset_valid(
        "snake16_psb_cmyk32_missing_composite_multilayer_normal.psd",
        "snake16_psb_cmyk32_missing_composite_multilayer_normal_high_offset_xxxxxxxlarge.psd",
        padding_bytes=0x7FF0000,
    )
    mutate_psb_high_offset_valid(
        "snake16_psb_mode7_cmyk32_missing_composite_multilayer_normal.psd",
        "snake16_psb_mode7_cmyk32_missing_composite_multilayer_normal_high_offset_xxxxxxxlarge.psd",
        padding_bytes=0x7FF0000,
    )
    mutate_psb_high_offset_valid(
        "snake16_psb_cmyk32_missing_composite_multilayer_normal_rle.psd",
        "snake16_psb_cmyk32_missing_composite_multilayer_normal_rle_high_offset_xxxxxxxlarge.psd",
        padding_bytes=0x7FF0000,
    )
    mutate_psb_high_offset_valid(
        "snake16_psb_mode7_cmyk32_missing_composite_multilayer_normal_rle.psd",
        "snake16_psb_mode7_cmyk32_missing_composite_multilayer_normal_rle_high_offset_xxxxxxxlarge.psd",
        padding_bytes=0x7FF0000,
    )
    mutate_psb_layer_info_end_overrun(
        "snake16_psb_cmyk16_missing_composite_multilayer_normal_high_offset_xxxxxxxlarge.psd",
        "snake16_psb_cmyk16_missing_composite_multilayer_normal_high_offset_xxxxxxxlarge_layer_info_end_overrun.psd",
    )
    mutate_psb_layer_info_end_overrun(
        "snake16_psb_mode7_cmyk16_missing_composite_multilayer_normal_high_offset_xxxxxxxlarge.psd",
        "snake16_psb_mode7_cmyk16_missing_composite_multilayer_normal_high_offset_xxxxxxxlarge_layer_info_end_overrun.psd",
    )
    mutate_psb_layer_info_end_overrun(
        "snake16_psb_cmyk32_missing_composite_multilayer_normal_high_offset_xxxxxxxlarge.psd",
        "snake16_psb_cmyk32_missing_composite_multilayer_normal_high_offset_xxxxxxxlarge_layer_info_end_overrun.psd",
    )
    mutate_psb_layer_info_end_overrun(
        "snake16_psb_mode7_cmyk32_missing_composite_multilayer_normal_high_offset_xxxxxxxlarge.psd",
        "snake16_psb_mode7_cmyk32_missing_composite_multilayer_normal_high_offset_xxxxxxxlarge_layer_info_end_overrun.psd",
    )
    mutate_psb_channel_window_overrun(
        "snake16_psb_cmyk16_missing_composite_multilayer_normal_high_offset_xxxxxxxlarge.psd",
        "snake16_psb_cmyk16_missing_composite_multilayer_normal_high_offset_xxxxxxxlarge_channel_window_overrun.psd",
    )
    mutate_psb_channel_window_overrun(
        "snake16_psb_mode7_cmyk16_missing_composite_multilayer_normal_high_offset_xxxxxxxlarge.psd",
        "snake16_psb_mode7_cmyk16_missing_composite_multilayer_normal_high_offset_xxxxxxxlarge_channel_window_overrun.psd",
    )
    mutate_psb_channel_window_overrun(
        "snake16_psb_cmyk32_missing_composite_multilayer_normal_high_offset_xxxxxxxlarge.psd",
        "snake16_psb_cmyk32_missing_composite_multilayer_normal_high_offset_xxxxxxxlarge_channel_window_overrun.psd",
    )
    mutate_psb_channel_window_overrun(
        "snake16_psb_mode7_cmyk32_missing_composite_multilayer_normal_high_offset_xxxxxxxlarge.psd",
        "snake16_psb_mode7_cmyk32_missing_composite_multilayer_normal_high_offset_xxxxxxxlarge_channel_window_overrun.psd",
    )
    mutate_psb_rle_payload_window_overrun(
        "snake16_psb_cmyk16_missing_composite_multilayer_normal_rle_high_offset_xxxxxxxlarge.psd",
        "snake16_psb_cmyk16_missing_composite_multilayer_normal_rle_high_offset_xxxxxxxlarge_rle_payload_window_overrun.psd",
    )
    mutate_psb_rle_payload_window_overrun(
        "snake16_psb_mode7_cmyk16_missing_composite_multilayer_normal_rle_high_offset_xxxxxxxlarge.psd",
        "snake16_psb_mode7_cmyk16_missing_composite_multilayer_normal_rle_high_offset_xxxxxxxlarge_rle_payload_window_overrun.psd",
    )
    mutate_psb_rle_payload_window_overrun(
        "snake16_psb_cmyk32_missing_composite_multilayer_normal_rle_high_offset_xxxxxxxlarge.psd",
        "snake16_psb_cmyk32_missing_composite_multilayer_normal_rle_high_offset_xxxxxxxlarge_rle_payload_window_overrun.psd",
    )
    mutate_psb_rle_payload_window_overrun(
        "snake16_psb_mode7_cmyk32_missing_composite_multilayer_normal_rle_high_offset_xxxxxxxlarge.psd",
        "snake16_psb_mode7_cmyk32_missing_composite_multilayer_normal_rle_high_offset_xxxxxxxlarge_rle_payload_window_overrun.psd",
    )
    mutate_psb_high_offset_valid(
        "snake16_psb_cmyk16_missing_composite_multilayer_normal.psd",
        "snake16_psb_cmyk16_missing_composite_multilayer_normal_high_offset_xxxxxxxlarge_over_limit.psd",
        padding_bytes=0x8000000,
    )
    mutate_psb_high_offset_valid(
        "snake16_psb_mode7_cmyk16_missing_composite_multilayer_normal.psd",
        "snake16_psb_mode7_cmyk16_missing_composite_multilayer_normal_high_offset_xxxxxxxlarge_over_limit.psd",
        padding_bytes=0x8000000,
    )
    mutate_psb_high_offset_valid(
        "snake16_psb_cmyk32_missing_composite_multilayer_normal.psd",
        "snake16_psb_cmyk32_missing_composite_multilayer_normal_high_offset_xxxxxxxlarge_over_limit.psd",
        padding_bytes=0x8000000,
    )
    mutate_psb_high_offset_valid(
        "snake16_psb_mode7_cmyk32_missing_composite_multilayer_normal.psd",
        "snake16_psb_mode7_cmyk32_missing_composite_multilayer_normal_high_offset_xxxxxxxlarge_over_limit.psd",
        padding_bytes=0x8000000,
    )
    mutate_psb_rle_row_table_too_short(
        "snake16_psb_cmyk8_missing_composite_multilayer_normal_rle.psd",
        "snake16_psb_cmyk8_missing_composite_multilayer_rle_row_table_too_short.psd",
    )
    mutate_psb_rle_row_length_overrun(
        "snake16_psb_cmyk8_missing_composite_multilayer_normal_rle.psd",
        "snake16_psb_cmyk8_missing_composite_multilayer_rle_row_length_overrun.psd",
    )
    mutate_psb_rle_row_table_too_short(
        "snake16_psb_cmyk32_missing_composite_multilayer_normal_rle.psd",
        "snake16_psb_cmyk32_missing_composite_multilayer_rle_row_table_too_short.psd",
    )
    mutate_psb_rle_row_length_overrun(
        "snake16_psb_cmyk32_missing_composite_multilayer_normal_rle.psd",
        "snake16_psb_cmyk32_missing_composite_multilayer_rle_row_length_overrun.psd",
    )
    mutate_psb_rle_row_table_too_short(
        "snake16_psb_mode7_cmyk8_missing_composite_multilayer_normal_rle.psd",
        "snake16_psb_mode7_cmyk8_missing_composite_multilayer_rle_row_table_too_short.psd",
    )
    mutate_psb_rle_row_length_overrun(
        "snake16_psb_mode7_cmyk8_missing_composite_multilayer_normal_rle.psd",
        "snake16_psb_mode7_cmyk8_missing_composite_multilayer_rle_row_length_overrun.psd",
    )


if __name__ == "__main__":
    main()
