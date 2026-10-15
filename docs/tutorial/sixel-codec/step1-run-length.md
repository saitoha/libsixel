# Step 1: A Complete Codec with Run-Length Coding

SIXEL looks obscure, but a working encoder and decoder are each about thirty lines of C. This step writes both, with no optimization beyond the repeat introducer that the format itself provides. The full source is [`code/step1.c`](code/step1.c).

## The idea in one picture

A SIXEL image is cut into horizontal *bands* six pixels high. One data character describes one column of one band for one color: bit *r* is set when the pixel in row *r* of the band has that color, and adding 63 (`?`) makes the six-bit value a printable character from `?` to `~`.

```text
band rows       column x         bits      character
row 0           color 2          0
row 1           color 5          1  <- color 5 in rows 1, 2 and 4
row 2           color 5          1
row 3           color 2          0         0b010110 = 22
row 4           color 5          1         22 + 63 = 85 = 'U'
row 5           color 2          0
```

To draw a band, select a color with `#n`, write one character per column, return to the left edge with `$`, select the next color, and so on. `-` moves to the next band. `!n` repeats the following character *n* times.

## Encoder

```c
static void flush(buffer_t *out, int ch, int count)
{
    if (count > 3) buf_printf(out, "!%d%c", count, ch);  /* DECGRI repeat */
    else while (count-- > 0) buf_putc(out, ch);
}

int tutorial_encode(image_t const *img, buffer_t *out)
{
    int w = img->width, h = img->height;

    buf_printf(out, "\033Pq\"1;1;%d;%d", w, h);          /* DCS and raster */
    for (int c = 0; c < img->ncolors; c++)               /* palette in % */
        buf_printf(out, "#%d;2;%d;%d;%d", c, PCT(img->palette[c * 3]),
                   PCT(img->palette[c * 3 + 1]), PCT(img->palette[c * 3 + 2]));
    for (int y = 0; y < h; y += 6) {                     /* one band */
        if (y > 0) buf_putc(out, '-');                   /* next band */
        for (int c = 0; c < img->ncolors; c++) {         /* one color */
            int last = -1, run = 0;
            buf_printf(out, c > 0 ? "$#%d" : "#%d", c);  /* CR, select */
            for (int x = 0; x < w; x++) {
                int bits = 0;
                for (int r = 0; r < 6 && y + r < h; r++)
                    if (img->pixels[(y + r) * w + x] == c) bits |= 1 << r;
                if (bits != last) flush(out, last + '?', run), run = 0;
                last = bits, run++;
            }
            if (last != 0) flush(out, last + '?', run);  /* drop blank tail */
        }
    }
    buf_printf(out, "\033\\");                           /* ST */
    return 0;
}
```

The structure follows the description above line by line:

- `ESC P q` opens the device control string. With no parameters, the pixel aspect ratio and background mode take their defaults, and the raster attributes `"1;1;W;H` declare square pixels and the image size.
- `#c;2;r;g;b` defines register *c* in RGB percentages. `PCT()` rounds an 8-bit channel to 0–100.
- For every band and every color, the inner loop computes the six-bit mask of each column and accumulates runs of equal characters. `flush()` writes a run as `!n` and the character when that is shorter than repeating the character.
- A run of `?` (no pixel of this color) at the end of a color's line is dropped: the next `$` returns to the left edge anyway.
- `ESC \` (string terminator, ST) ends the image.

Every column character is written once per color, including columns where the color is absent. The output is correct but grows with the number of colors, and so does the time, as the measurements below show.

## Decoder

```c
int tutorial_decode(unsigned char const *p, size_t size, image_t *img)
{
    unsigned char const *end = p + size;
    int a[5], x = 0, y = 0, color = 0, repeat = 1;

    while (p < end && *p++ != 'q') {}                    /* skip ESC P ... q */
    while (p < end && *p != 0x1b) {                      /* until ESC \ */
        int ch = *p++;
        if (ch == '"') {                                 /* raster attributes */
            parse_params(&p, end, a, 4);
            if (image_alloc(img, a[2], a[3]) != 0) return -1;
        } else if (ch == '#') {                          /* color register */
            int n = parse_params(&p, end, a, 5);
            color = a[0] > 255 ? 255 : a[0];
            if (color >= img->ncolors) img->ncolors = color + 1;
            if (n == 5 && a[1] == 2) set_rgb_pct(img, color, a[2], a[3], a[4]);
        } else if (ch == '!') {                          /* repeat count */
            parse_params(&p, end, &repeat, 1);
        } else if (ch == '$') {                          /* carriage return */
            x = 0;
        } else if (ch == '-') {                          /* next band */
            x = 0, y += 6;
        } else if (ch >= '?' && ch <= '~') {             /* six pixels */
            for (int i = 0; i < (repeat > 0 ? repeat : 1); i++, x++)
                for (int r = 0; r < 6; r++)
                    if ((ch - '?') >> r & 1 && x < img->width && y + r < img->height)
                        img->pixels[(y + r) * img->width + x] = (unsigned char)color;
            repeat = 1;
        }
    }
    return img->pixels != NULL ? 0 : -1;
}
```

The decoder is a loop over bytes with one branch per control function. It keeps four pieces of state: the cursor `x`, the band's top row `y`, the selected `color` and a pending `repeat` count. The data-character branch writes every set bit of every repeated column, and the bounds test keeps paint outside the declared raster from touching memory. `parse_params()` in the harness reads `;`-separated decimal numbers.

This decoder is intentionally incomplete. It needs raster attributes to size the image and clips paint outside them, it ignores HLS color definitions, and it makes up to six scattered stores per data character. A stream from another encoder may rely on all three; step 2 completes the syntax.

## What it costs

<!-- measurements:begin -->
| Input | Encode | Decode | Stream |
| --- | --- | --- | --- |
| Photograph, 600×450 | 21.0 ms | 2.42 ms | 275,844 bytes |
| Photograph, 2400×1800 | 319.8 ms | 33.0 ms | 3,019,374 bytes |
| Two colors, 2400×1800 | 3.52 ms | 4.61 ms | 1,441,842 bytes |
| Random noise, 2400×1800 | 592.9 ms | 373.7 ms | 20,629,898 bytes |
<!-- measurements:end -->

For each band, the encoder visits every pixel once per palette color: the work is proportional to colors × pixels. With two colors this is cheap, which is why step 1 is fast on the two-color pattern, but the 216-color photograph of the same size takes about 90 times as long. The stream also carries a `$` and a `#n` for every color of every band, and blank runs for colors that appear only at the right of a band. [Step 2](step2-nodes.md) removes both costs.
