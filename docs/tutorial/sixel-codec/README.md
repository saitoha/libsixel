# Writing a SIXEL Codec, Step by Step

SIXEL is a way to send pictures to a terminal as plain text. In this tutorial you write two small programs: an **encoder** that turns a picture file into SIXEL text, and a **decoder** that turns SIXEL text back into a picture file. You will see your own pictures in the terminal from the very first lesson. After that, the steps make the programs faster, one idea at a time, until their core loops are close to libsixel's own in speed.

![An encoder turns a small tree picture into a short line of SIXEL text, and a decoder turns the text back into the same picture](figures/big-picture.svg)

You do not need libsixel, and you do not need to know anything about image formats. If you can write a loop over an array in C and know what `|`, `&` and `<<` do, you are ready. [Lesson 0](step0-by-hand.md) needs no programming at all: you draw a picture with a pencil and print it with `printf`.

## Your first picture in one minute

The programs live in the `code` directory, next to this page's [makefile](code/Makefile). Build them and show a photograph:

```sh
cd docs/tutorial/sixel-codec/code
make
./step1 encode ../../../../images/snake.ppm
```

If your terminal understands SIXEL, a snake appears. Terminals that do include xterm (started as `xterm -ti vt340`), mlterm, foot, WezTerm, iTerm2, Konsole and recent Windows Terminal. If yours does not, nothing is lost: the decoder turns SIXEL back into an ordinary image file that any image viewer opens.

```sh
./step1 encode ../../../../images/snake.ppm > snake.six   # save the text
./step1 decode snake.six > snake.ppm                      # and turn it back
```

## The programs you will use

Every step builds one program with the same commands:

| Command | What it does |
| --- | --- |
| `./stepN encode picture.ppm` | Reads a picture and writes SIXEL to the terminal (standard output) |
| `./stepN decode picture.six > out.ppm` | Reads SIXEL and writes a PPM picture to standard output |
| `./stepN encode picture.ppm \| ./stepN decode > out.ppm` | Without a file name, both commands read standard input, so they work in pipes |
| `./stepN selftest` | Encodes and decodes 50 test pictures and checks that every pixel comes back |
| `./stepN bench picture.ppm` | Measures how fast encoding and decoding are |

The encoder and decoder of different steps can be mixed freely: `./step1 encode x.ppm | ./step6 decode > y.ppm` works, because they all speak the same SIXEL.

### Pictures as PNM files

The programs read and write **PNM** files, the simplest picture format there is: a short text header followed by the pixels, row after row, exactly as they sit in memory.

![A heart written as a text P2 file and as a binary P5 file; after the header the P5 file holds one byte per pixel, which is exactly the indexed image the codec works on](figures/pnm-layout.svg)

- **PGM** (`P2` as text, `P5` as binary) is a gray picture with one number per pixel. The programs use that number directly as the pixel's **color register**, so a `P5` file's pixel bytes are, unchanged, the memory your encoder reads. Because `P2` is plain text, you can draw a picture in a text editor, as Lesson 0 does.
- **PPM** (`P3` as text, `P6` as binary) has three numbers per pixel: red, green and blue. If a picture uses at most 256 colors, each color gets a register of its own. Otherwise the program picks the nearest of 216 fixed colors and adds a fine pattern of dots (*dithering*) so that the colors in between still look right. Choosing colors well is a big subject of its own, covered from the [palette construction pipeline](../../encoder/palette-construction/pipeline/README.md) onward; this tutorial keeps it out of the way.
- The decoder always writes a binary `P6` file: for every pixel, the red, green and blue of its register.

## Your part: two functions

Everything around the codec, reading files, writing files and the command line, is already written in [`code/common.c`](code/common.c). Your job is the codec itself: two functions, declared in [`code/tutorial.h`](code/tutorial.h).

```c
typedef struct image {
    int width;
    int height;
    int ncolors;                    /* registers in use, 1 to 256 */
    unsigned char *pixels;          /* width * height register numbers */
    unsigned char palette[256 * 3]; /* red, green, blue of each register */
} image_t;

int encode_sixel(image_t const *img, buffer_t *out);
int decode_sixel(unsigned char const *data, size_t size, image_t *img);
```

The pixel at column `x` and row `y` is `img->pixels[y * img->width + x]`, a register number from 0 to 255. The encoder appends SIXEL text to `out` with `buf_printf()`, which works like `printf()` but writes into memory, and `buf_putc()`, which adds one character. The program then writes `out` to the terminal. The decoder gets the whole SIXEL text in memory and fills `img`.

[`code/mysixel.c`](code/mysixel.c) is a ready-made starting point with both functions empty and hints in the comments:

```sh
make mysixel
./mysixel encode heart.pgm        # your encoder
./mysixel selftest                # are all 50 pictures right?
```

Steps 1 to 6 are worked answers. Try a part yourself first, then compare with the step's code.

## The journey

![A staircase of seven lessons: 0 read SIXEL by hand, 1 a simple codec, 2 nodes, 3 fewer decisions, 4 64-bit words, 5 SIMD, 6 threads](figures/journey.svg)

| Lesson | You learn | Encoder | Decoder |
| --- | --- | --- | --- |
| [0. By hand](step0-by-hand.md) | How SIXEL draws: six-pixel columns, colors, bands | You, with a pencil | Your terminal |
| [1. A first codec](step1-run-length.md) | The whole format in about thirty lines each way | Looks at the band once per color | Paints one pixel at a time |
| [2. Nodes](step2-nodes.md) | Reading each pixel only once; packing colors onto shared lines | One pass per band, kmiya's nodes | Full syntax; fills repeats a row at a time |
| [3. Fewer decisions](step3-fewer-decisions.md) | Why a question can cost more than the work it decides | A bitmap finds the next node | Stays in a loop over data characters; masks instead of `if` |
| [4. 64-bit words](step4-column-words.md) | Changing how data sits in memory; bit tricks on whole words | Six pixels in one number; bitmaps of occupied columns | One store per character |
| [5. SIMD](step5-simd.md) | Vector instructions that work on 16 bytes at once | Sixteen columns at once | Turns columns back into rows quickly |
| [6. Threads](step6-threads.md) | Splitting work between CPU cores | Bands in parallel | Finds the bands first, then decodes them in parallel |

Each lesson follows the same pattern: the problem, the idea in a picture, a small example you can follow by hand, the code, things to try yourself, and how fast the result is.

## Checking your work

Steps 2 to 6 must write **exactly the same bytes** for every picture, and decoders 2 to 6 must produce exactly the same pixels from every stream. The step 1 decoder is deliberately simpler: it agrees with them on the streams these encoders write, but it needs a size and does not understand HLS colors. That makes each later step a pure speed-up, and it makes mistakes easy to catch:

```sh
make check      # all self tests and comparisons between the steps
make bench      # time every step on the pictures of the table below
```

[`code/check.sh`](code/check.sh) round-trips 50 test pictures of awkward sizes through every step, requires steps 2 to 6 to write identical bytes with any number of threads, reads the same picture as text `P2` and binary `P5`, and feeds every decoder the sample streams in `images/` and hand-written streams that leave out the size, paint beyond it, use HLS colors or change a color after using it. A round trip alone is not enough: a mistake can produce a longer but still correct stream, and only comparing the bytes notices.

## How fast does it get?

![Bar chart of Full HD photographs encoded and decoded per second at each step, rising steeply at step 6](figures/speed.svg)

The table gives the time to encode and to decode one picture, in milliseconds, as measured by `make bench`. The inputs are two photographs with an adaptive 256-color palette and error diffusion, the kind of picture `img2sixel` writes, a 1920×1080 pattern of two colors, and 1920×1080 random pixels in 256 colors, the hardest case.

<!-- measurements:begin:summary -->
| Step | Photograph, 600×450 | Photograph, 1920×1080 | Two colors, 1920×1080 | Random noise, 1920×1080 |
| --- | --- | --- | --- | --- |
| 1 | 25.6 / 7.43 | 183.1 / 57.0 | 1.70 / 2.09 | 276.9 / 170.2 |
| 2 | 3.88 / 2.00 | 22.1 / 11.5 | 2.32 / 1.83 | 155.3 / 32.8 |
| 3 | 3.65 / 0.76 | 21.4 / 4.17 | 2.17 / 1.47 | 103.2 / 18.6 |
| 4 | 2.30 / 0.59 | 11.8 / 3.07 | 3.90 / 1.13 | 68.5 / 15.7 |
| 5 | 2.31 / 0.51 | 10.7 / 2.67 | 2.70 / 0.70 | 72.9 / 15.2 |
| 6 | 2.36 / 0.13 | 2.39 / 0.53 | 0.57 / 0.33 | 14.6 / 2.30 |
<!-- measurements:end -->

Each cell is encode / decode time in milliseconds; smaller is faster. Stream sizes are listed on the lesson pages; steps 2 to 6 write identical streams. The numbers come from one Apple M3 Max, and the [measurement record](measurements.md) gives the conditions, the time of each phase, the thread sweep and the commands to reproduce them.

Three lessons hide in these numbers. The biggest single jump in the encoder comes from a better plan, not from clever instructions: step 1 looks at every pixel once per color, step 2 only once. Once the plan is right, the biggest gain on one core comes from simply asking fewer questions, with no change of data layout: step 3 makes the photograph's decoder 2.7 times faster. And the cleverest instructions, in step 5, give the smallest gain on photographs, because the time is mostly spent elsewhere. Some ideas even cost time on some pictures. That is why every step is measured instead of assumed to be faster.

## Words used in this tutorial

| Word | Meaning |
| --- | --- |
| Pixel | One dot of the picture |
| Register | A numbered color slot, 0 to 255; a pixel stores a register number, not a color |
| Palette | The colors held by the registers |
| Band | Six rows of pixels, the height SIXEL paints at once |
| Sixel character | One of the 64 characters `?` to `~`; it says which of the six pixels of one column get the current color |
| Graphics line | One trip of the cursor from the left edge to the right inside a band |
| Stream | The complete SIXEL text of a picture |
| Node | A stretch of one color within a band (step 2) |
| Branch | A place where the program chooses between two paths, such as an `if`; the processor guesses the answer (step 3) |
| Column word | The six pixels of one column packed into one 64-bit number (step 4) |
| Lane | One of the sixteen bytes of a vector register (step 5) |
| Thread | A part of the program that can run on a CPU core at the same time as the others (step 6) |

## From the tutorial to libsixel

The tutorial codecs are deliberately small. On the same indexed pictures, the tutorial decoders [keep pace with libsixel's](measurements.md#comparison-with-libsixel), and the encoders are at most about 1.3 times slower, but libsixel's [encoder](../../encoder/pipeline/README.md) and [decoder](../../decoder/pipeline/README.md) also handle transparency, OR mode, high-color and 6delta output, the size-oriented [encoding policy](../../encoder/output/encode-policy/README.md), streaming output, palette history during decoding, and the many [robustness](../../sixel-format/README.md#robustness-and-security) concerns of untrusted input. The [SIXEL format overview](../../sixel-format/README.md) is the full reference for the format. Each lesson ends by pointing to the part of libsixel where its idea appears with all of those concerns included.

The figures are generated by [`tools/plot_sixel_codec_tutorial_figures.py`](../../../tools/plot_sixel_codec_tutorial_figures.py), which computes every SIXEL fragment it draws with the same rules as the tutorial code. Regenerate them with `tools/reproduce_sixel_codec_tutorial_figures.sh`; pass `--check` to verify that they are up to date.
