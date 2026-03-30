#!/usr/bin/env python3
"""Generate PSB (8BPB/version2) fixtures from PSD missing-composite fixtures.

This script converts layer-only PSD fixtures that use 32-bit layer lengths into
PSB layout (64-bit layer/mask length fields, 64-bit per-channel lengths).
Optionally, it can rewrite per-layer channel payloads to RLE with PSB 4-byte
row length tables.
"""

from __future__ import annotations

import pathlib
import struct


HERE = pathlib.Path(__file__).resolve().parent


def read_u16be(data: bytes, off: int) -> int:
    return struct.unpack_from(">H", data, off)[0]


def read_i16be(data: bytes, off: int) -> int:
    return struct.unpack_from(">h", data, off)[0]


def read_u32be(data: bytes, off: int) -> int:
    return struct.unpack_from(">I", data, off)[0]


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


def main() -> None:
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
        "snake16_cmyk16_missing_composite_multilayer_normal_valid_icc_profile.psd",
        "snake16_psb_cmyk16_missing_composite_multilayer_normal_valid_icc_profile.psd",
    )
    convert_fixture(
        "snake16_cmyk16_missing_composite_multilayer_normal_bad_icc_profile.psd",
        "snake16_psb_cmyk16_missing_composite_multilayer_normal_bad_icc_profile.psd",
    )
    convert_fixture(
        "snake16_cmyk16_missing_composite_multilayer_normal_malformed_resource.psd",
        "snake16_psb_cmyk16_missing_composite_multilayer_normal_malformed_resource.psd",
    )
    convert_fixture(
        "snake16_mode7_cmyk32_missing_composite_multilayer_normal_valid_icc_profile.psd",
        "snake16_psb_mode7_cmyk32_missing_composite_multilayer_normal_valid_icc_profile.psd",
    )
    convert_fixture(
        "snake16_mode7_cmyk32_missing_composite_multilayer_normal_bad_icc_profile.psd",
        "snake16_psb_mode7_cmyk32_missing_composite_multilayer_normal_bad_icc_profile.psd",
    )
    convert_fixture(
        "snake16_mode7_cmyk32_missing_composite_multilayer_normal_malformed_resource.psd",
        "snake16_psb_mode7_cmyk32_missing_composite_multilayer_normal_malformed_resource.psd",
    )

    # RLE row-table(4-byte) path fixture for PSB layer channel decoding.
    convert_fixture(
        "snake16_rgb8_missing_composite_multilayer_normal.psd",
        "snake16_psb_rgb8_missing_composite_multilayer_normal_rle.psd",
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


if __name__ == "__main__":
    main()
