# Builtin TGA Loader

## Identity and history

TGA remains in the adapted stb-derived core imported with commit [`81375f0bd`](https://github.com/saitoha/libsixel/commit/81375f0bd). It has no strong leading file signature, so recognition validates the 18-byte header and its compatible image-type, colormap, size, and depth fields. Commit [`6d57f075d`](https://github.com/saitoha/libsixel/commit/6d57f075d) added the palette-preserving fast path on 2026-01-22, and [`ba59c4607`](https://github.com/saitoha/libsixel/commit/ba59c4607) added explicit truecolor alpha-policy handling on 2026-03-29.

## Accepted variants

The decoder accepts colormapped image types 1 and 9, truecolor types 2 and 10, and grayscale types 3 and 11; adding 8 to the base type selects TGA RLE. Colormap entries may be 8, 15, 16, 24, or 32 bits. Colormap indices may be 8 or 16 bits in the general path, while the palette-preserving `PAL8` path requires 8-bit indices. Direct samples may be 8-bit gray, 16-bit gray plus alpha, 15/16-bit 5:5:5 RGB, 24-bit BGR, or 32-bit BGRA. Vertical origin is honored.

Indexed 8-bit input can remain `PAL8`; 32-bit palette entries provide transparency metadata, and multiple fully transparent entries are folded into the frame's single transparent palette key when necessary. Other forms become gamma RGB/RGBA byte data. Truecolor alpha is handled by common background/alpha policy.

## Not supported or not interpreted

The x/y origin fields are parsed but do not place the image on a larger canvas, and a nonzero first-colormap-entry index is not implemented as a palette index base. Horizontal right-to-left origin is not implemented. The descriptor's claimed alpha-bit count is not used to reinterpret 15/16-bit truecolor; those forms are treated as opaque RGB, while explicit 16-bit grayscale-alpha and 32-bit pixels carry alpha through their component layouts. TGA 2.0 extension/developer areas, color-correction tables, scan-line tables, thumbnails, and ICC/profile metadata are not interpreted. The loader is single-frame and eight-bit at its output boundary.
