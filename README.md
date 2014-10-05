libsixel
========

[![Build Status](https://travis-ci.org/saitoha/libsixel.svg?branch=master)](https://travis-ci.org/saitoha/libsixel)
[![Coverage Status](https://coveralls.io/repos/saitoha/libsixel/badge.png?branch=master)](https://coveralls.io/r/saitoha/libsixel?branch=master)

## What is this?

This package provides encoder/decoder implementation for DEC SIXEL graphics, and
some converter programs.

![img2sixel](https://raw.githubusercontent.com/saitoha/libsixel/data/data/libsixel-1.png)

SIXEL is one of image formats for printer and terminal imaging introduced by
Digital Equipment Corp. (DEC).
Its data scheme is represented as a terminal-friendly escape sequence.
So if you want to view a SIXEL image file, all you have to do is "cat" it to your terminal.

## SIXEL Animation

img2sixel(1) can decode GIF animation.

  ![Animation](https://raw.githubusercontent.com/saitoha/libsixel/data/data/sixel.gif)


## Related projects

### Video streaming

Now Youtube video streaming is available over SIXEL protocol by [FFmpeg-SIXEL](https://github.com/saitoha/FFmpeg-SIXEL) project.

  [![FFmpeg-SIXEL](https://raw.githubusercontent.com/saitoha/libsixel/data/data/ffmpeg.png)](http://youtu.be/hqMh47lYHlc)


### SDL integration: Gaming, Virtualization, ...etc.

[SDL1.2-SIXEL](https://github.com/saitoha/SDL1.2-SIXEL) project makes enable you to operate various GUI applications on the terminal.

You can play "The Battle for Wesnoth" over SIXEL protocol.

  [![SDL1.2-SIXEL WESNOTH](https://raw.githubusercontent.com/saitoha/libsixel/data/data/wesnoth.png)](http://youtu.be/aMUkN7TSct4)

You can run QEMU on SIXEL terminals.

  [![SDL1.2-SIXEL QEMU](https://raw.githubusercontent.com/saitoha/libsixel/data/data/qemu.png)](http://youtu.be/X6M5tgNjEuQ)

[SDL1.2-SIXEL](https://github.com/saitoha/SDL1.2-SIXEL) can collaborate with [XSDL-SIXEL](https://github.com/saitoha/xserver-xsdl-sixel).

  [![SDL1.2-SIXEL XSDL](https://raw.githubusercontent.com/saitoha/libsixel/data/data/xsdl.png)](http://youtu.be/UOTMGdUPYRo)


### X11 on SIXEL terminals

[XSIXEL](https://github.com/saitoha/xserver-sixel) is a kdrive server implementation for SIXEL terminals.

  ![XSIXEL](https://raw.githubusercontent.com/saitoha/libsixel/data/data/xsixel.png)


### W3M integration

img2sixel(1) can be integrated with ![Arakiken's w3m fork(remoteimg branch)](https://bitbucket.org/arakiken/w3m/branch/remoteimg).

  ![w3m-sixel](https://raw.githubusercontent.com/saitoha/libsixel/data/data/w3m-sixel.png)


### Twitter client integration

  Now [mikutter](http://mikutter.hachune.net/) + [mikutterm](https://bitbucket.org/arakiken/mikutterm) works with libsixel inline-image extension.



## Terminal requirements

If you want to view a SIXEL image, you have to get a terminal which support sixel graphics.

Now SIXEL feature is supported by the following terminals.

- DEC VT series, VT240/VT241/VT330/VT340/VT282/VT284/VT286/VT382

- DECterm(dxterm)

- Kermit

- ZSTEM 340

- WRQ Reflection

- RLogin (Japanese terminal emulator)

  [http://nanno.dip.jp/softlib/man/rlogin/](http://nanno.dip.jp/softlib/man/rlogin/)

- tanasinn (Works with firefox)

  [http://zuse.jp/tanasinn/](http://zuse.jp/tanasinn/)

- mlterm

  [http://mlterm.sourceforge.net/](http://mlterm.sourceforge.net/)

  Works on each of X, win32/cygwin, framebuffer version.

- XTerm (compiled with --enable-sixel option)

  [http://invisible-island.net/xterm/](http://invisible-island.net/xterm/)

  You should launch xterm with "-ti 340" option. the SIXEL palette is limited to a maximum of 16 colors.

- yaft (in github repo)

  [https://github.com/uobikiemukot/yaft](https://github.com/uobikiemukot/yaft)

- recterm (ttyrec to GIF converter)

  [https://github.com/uobikiemukot/recterm](https://github.com/uobikiemukot/recterm)

- seq2gif (ttyrec to GIF converter)

  [https://github.com/saitoha/seq2gif](https://github.com/saitoha/seq2gif)


## Quantization quality

img2sixel(1) supports high quality color image quantization.

- ppmtosixel (netpbm)

    $ jpegtopnm images/snake.jpg | pnmquant 16 | ppmtosixel

  ![ppmtosixel](https://raw.githubusercontent.com/saitoha/libsixel/data/data/q_ppmtosixel.png)


- ppmtosixel with Floyd–Steinberg dithering (netpbm)

    $ jpegtopnm images/snake.jpg | pnmquant 16 -floyd | ppmtosixel

  ![ppmtosixel](https://raw.githubusercontent.com/saitoha/libsixel/data/data/q_ppmtosixel2.png)


- kmiya's sixel

    $ sixel -p16 images/snake.jpg

  ![kmiya's sixel](https://raw.githubusercontent.com/saitoha/libsixel/data/data/q_sixel.png)


- PySixel (sixelconv command)

    $ sixelconv -n16 images/snake.jpg

  ![PySixel](https://raw.githubusercontent.com/saitoha/libsixel/data/data/q_sixelconv.png)


- libsixel (img2sixel command)

    $ img2sixel -p16 images/snake.jpg

  ![PySixel](https://raw.githubusercontent.com/saitoha/libsixel/data/data/q_libsixel.png)


## Build and install

```
$ ./configure
$ make
# make install
```

## Configure options

### Build with optional packages

```
--with-gdk-pixbuf2        build with gdk-pixbuf2 (default: no)
--with-libcurl            build with libcurl (default: no)
--with-gd                 build with gd (default: no)
--with-pkgconfigdir       specify pkgconfig dir (default is libdir/pkgconfig)
--with-bashcompletiondir  specify bashcompletion.d
--with-zshcompletiondir   specify zshcompletion.d
```

## Usage of command line tools

### img2sixel

```
Usage: img2sixel [Options] imagefiles
       img2sixel [Options] < imagefile

Options:
-7, --7bit-mode            generate a sixel image for 7bit
                           terminals or printers (default)
-8, --8bit-mode            generate a sixel image for 8bit
                           terminals or printers
-p COLORS, --colors=COLORS specify number of colors to reduce
                           the image to (default=256)
-m FILE, --mapfile=FILE    transform image colors to match this
                           set of colorsspecify map
-e, --monochrome           output monochrome sixel image
                           this option assumes the terminal
                           background color is black
-i, --invert               assume the terminal background color
                           is white, make sense only when -e
                           option is given
-u, --use-macro            use DECDMAC and DEVINVM sequences to
                           optimize GIF animation rendering
-n, --macro-number         specify an number argument for
                           DECDMAC and make terminal memorize
                           SIXEL image. No image is shown if this
                           option is specified
-g, --ignore-delay         render GIF animation without delay
-d DIFFUSIONTYPE, --diffusion=DIFFUSIONTYPE
                           choose diffusion method which used
                           with -p option (color reduction)
                           DIFFUSIONTYPE is one of them:
                             auto     -> choose diffusion type
                                         automatically (default)
                             none     -> do not diffuse
                             fs       -> Floyd-Steinberg method
                             atkinson -> Bill Atkinson's method
                             jajuni   -> Jarvis, Judice & Ninke
                             stucki   -> Stucki's method
                             burkes   -> Burkes' method
-f FINDTYPE, --find-largest=FINDTYPE
                           choose method for finding the largest
                           dimention of median cut boxes for
                           splitting, make sense only when -p
                           option (color reduction) is
                           specified
                           FINDTYPE is one of them:
                             auto -> choose finding method
                                     automatically (default)
                             norm -> simply comparing the
                                     range in RGB space
                             lum  -> transforming into
                                     luminosities before the
                                     comparison
-s SELECTTYPE, --select-color=SELECTTYPE
                           choose the method for selecting
                           representative color from each
                           median-cut box, make sense only
                           when -p option (color reduction) is
                           specified
                           SELECTTYPE is one of them:
                             auto     -> choose selecting
                                         method automatically
                                         (default)
                             center   -> choose the center of
                                         the box
                             average  -> caclulate the color
                                         average into the box
                             histgram -> similar with average
                                         but considers color
                                         histgram
-c REGION, --crop=REGION   crop source image to fit the
                           specified geometry. REGION should
                           be formatted as '%dx%d+%d+%d'
-w WIDTH, --width=WIDTH    resize image to specified width
                           WIDTH is represented by the
                           following syntax
                             auto       -> preserving aspect
                                           ratio (default)
                             <number>%  -> scale width with
                                           given percentage
                             <number>   -> scale width with
                                           pixel counts
                             <number>px -> scale width with
                                           pixel counts
-h HEIGHT, --height=HEIGHT resize image to specified height
                           HEIGHT is represented by the
                           following syntax
                             auto       -> preserving aspect
                                           ratio (default)
                             <number>%  -> scale height with
                                           given percentage
                             <number>   -> scale height with
                                           pixel counts
                             <number>px -> scale height with
                                           pixel counts
-r RESAMPLINGTYPE, --resampling=RESAMPLINGTYPE
                           choose resampling filter used
                           with -w or -h option (scaling)
                           RESAMPLINGTYPE is one of them:
                             nearest  -> Nearest-Neighbor
                                         method
                             gaussian -> Gaussian filter
                             hanning  -> Hanning filter
                             hamming  -> Hamming filter
                             bilinear -> Bilinear filter
                                         (default)
                             welsh    -> Welsh filter
                             bicubic  -> Bicubic filter
                             lanczos2 -> Lanczos-2 filter
                             lanczos3 -> Lanczos-3 filter
                             lanczos4 -> Lanczos-4 filter
-q QUALITYMODE, --quality=QUALITYMODE
                           select quality of color
                           quanlization.
                             auto -> decide quality mode
                                     automatically (default)
                             high -> high quality and low
                                     speed mode
                             low  -> low quality and high
                                     speed mode
-l LOOPMODE, --loop-control=LOOPMODE
                           select loop control mode for GIF
                           animation.
                             auto   -> honor the setting of
                                       GIF header (default)
                             force   -> always enable loop
                             disable -> always disable loop
-V, --version              show version and license info
-H, --help                 show this help
```

Convert a jpeg image file into a sixel file

```
$ img2sixel < images/egret.jpg > egret.sixel
```

Reduce colors to 16:

```
$ img2sixel -p 16 < images/egret.jpg > egret.sixel
```

Reduce colors with fixed palette:

```
$ img2sixel -m images/map16.png < images/egret.jpg > egret.sixel
```

### sixel2png

```
Usage: sixel2png -i input.sixel -o output.png
       sixel2png < input.sixel > output.png

Options:
-i, --input     specify input file
-o, --output    specify output file
-V, --version   show version and license information
-H, --help      show this help
```

Convert a sixel file into a png image file

```
$ sixel2png < egret.sixel > egret.png
```

## Usage of conversion API 1.0

The Whole API is described [here](https://github.com/saitoha/libsixel/blob/master/include/sixel.h.in).

### Example

If you use OSX, a tiny example is available
[here](https://github.com/saitoha/libsixel/blob/master/examples/osx/opengl/).

  ![opengl example](https://raw.githubusercontent.com/saitoha/libsixel/data/data/example_opengl.gif)

### Bitmap to SIXEL

*sixel_encode* function converts bitmap array into SIXEL format.

```C
/* converter API */

/* convert pixels into sixel format and write it to output context */
int
sixel_encode(unsigned char  /* in */ *pixels,   /* pixel bytes */
             int            /* in */  width,    /* image width */
             int            /* in */  height,   /* image height */
             int            /* in */  depth,    /* pixel depth: now only 3 is supported */
             sixel_dither_t /* in */ *dither,   /* dither context */
             sixel_output_t /* in */ *context); /* output context */
```
To use this function, you have to initialize two objects,

- *sixel_dither_t* (dithering context object)
- *sixel_output_t* (output context object)

#### Dithering context

Here is a part of APIs for dithering context manipulation.

```C
/* create dither context object: reference counter is set to 1 */
sixel_dither_t *
sixel_dither_create(int /* in */ ncolors); /* number of colors */

/* increment reference count of dither context object (thread-unsafe) */
void
sixel_dither_ref(sixel_dither_t *dither); /* dither context object */

/* decrement reference count of dither context object (thread-unsafe) */
void
sixel_dither_unref(sixel_dither_t *dither); /* dither context object */

/* initialize internal palette from specified pixel buffer */
int
sixel_dither_initialize(
    sixel_dither_t *dither,          /* dither context object */
    unsigned char /* in */ *data,    /* sample image */
    int /* in */ width,              /* image width */
    int /* in */ height,             /* image height */
    int /* in */ depth,              /* pixel depth, now only '3' is supported */
    int /* in */ method_for_largest, /* set 0 or method for finding the largest dimention */
    int /* in */ method_for_rep,     /* set 0 or method for choosing a color from the box */
    int /* in */ quality_mode        /* set 0 or quality of histgram processing */
);
```

#### Output context

Here is a part of APIs for output context manipulation.

```C
typedef int (* sixel_write_function)(char *data, int size, void *priv);

/* create output context object */
sixel_output_t *const
sixel_output_create(
    sixel_write_function /* in */ fn_write, /* callback function for output sixel */
    void /* in */ *priv                     /* private data given as
);                                             3rd argument of fn_write */

/* increment reference count of output context object (thread-unsafe) */
void
sixel_output_ref(sixel_output_t /* in */ *output);     /* output context */

/* decrement reference count of output context object (thread-unsafe) */
void
sixel_output_unref(sixel_output_t /* in */ *output);   /* output context */

```

### SIXEL to indexed bitmap

*sixel_decode* function converts SIXEL into indexed bitmap bytes with its palette.

```
/* malloc(3) compatible function */
typedef void * (* sixel_allocator_function)(size_t size);

/* convert sixel data into indexed pixel bytes and palette data */
int
sixel_decode(unsigned char              /* in */  *sixels,    /* sixel bytes */
             int                        /* in */  size,       /* size of sixel bytes */
             unsigned char              /* out */ **pixels,   /* decoded pixels */
             int                        /* out */ *pwidth,    /* image width */
             int                        /* out */ *pheight,   /* image height */
             unsigned char              /* out */ **palette,  /* RGBA palette */
             int                        /* out */ *ncolors,   /* palette size (<= 256) */
             sixel_allocator_function   /* out */ allocator); /* malloc function */
```


## Support

This software is provided "as is" without express or implied warranty.
The main support channel for this software is the github issue tracker:

  [https://github.com/saitoha/libsixel/issues](https://github.com/saitoha/libsixel/issues)


Please post your issue if you have any problems, questions or suggestions.


## License

The MIT License (MIT)

> Copyright (c) 2014 Hayaki Saito
>
> Permission is hereby granted, free of charge, to any person obtaining a copy of
> this software and associated documentation files (the "Software"), to deal in
> the Software without restriction, including without limitation the rights to
> use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of
> the Software, and to permit persons to whom the Software is furnished to do so,
> subject to the following conditions:
>
> The above copyright notice and this permission notice shall be included in all
> copies or substantial portions of the Software.
>
> THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
> IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS
> FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR
> COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER
> IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
> CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

## Acknowledgment

This software derives from the following implementations.

### sixel 2014-3-2

src/tosixel.c, src/fromsixel.c, and some part of converters/loader.c are
derived from kmiya's "*sixel*" original version (2014-3-2)

  Package: http://nanno.dip.jp/softlib/man/rlogin/sixel.tar.gz

  Unofficial repo: https://github.com/saitoha/sixel

This work is written by kmiya@ culti. He distributes it under very permissive license.

The original license text(in Japanese only) is:

```
このプログラム及びソースコードの使用について個人・商用を問わず

ご自由に使用していただいで結構です。

また、配布・転載・紹介もご連絡の必要もありません。

ソースの改変による配布も自由ですが、どのバージョンの改変かを

明記されることを希望します。

バージョン情報が無い場合は、配布物の年月日を明記されることを

希望します。

                  2014/10/05  kmiya
```

The unofficial translation:

```
Anyone is free to use this program for any purpose,
commercial or non-commercial, without any restriction.

Anyone is free to distribute, copy, publish, or
advertise this software, without any contact.

Anyone is free to distribute with modification of the
source code, but I "hope" that its based version or
date will be written clearly.

                                    2014/10/05 kmiya
```

kmiya also said this is compatible with MIT/BSD/GPL.

### stbi-1.41

This software includes *stbi-1.41* (stb_image.h),
public domain JPEG/PNG reader.

https://github.com/nothings/stb


### stbiw-0.92

This software includes *stbiw-0.94* (stb_image_write.h),
public domain PNG/BMP/TGA writer.

https://github.com/nothings/stb


### pnmquant.c (netpbm library)

The implementation of median cut algorithm for color quantization in quant.c
is imported from *pnmcolormap* included in *netpbm library*.

http://netpbm.sourceforge.net/

*pnmcolormap* was derived from *ppmquant*, originally written by Jef Poskanzer.

> Copyright (C) 1989, 1991 by Jef Poskanzer.
> Copyright (C) 2001 by Bryan Henderson.
>
> Permission to use, copy, modify, and distribute this software and its
> documentation for any purpose and without fee is hereby granted, provided
> that the above copyright notice appear in all copies and that both that
> copyright notice and this permission notice appear in supporting
> documentation.  This software is provided "as is" without express or
> implied warranty.


### ax_gcc_var_attribute / ax_gcc_func_attribute

These are useful m4 macros for detecting some GCC attributes.

http://www.gnu.org/software/autoconf-archive/ax_gcc_var_attribute.html
http://www.gnu.org/software/autoconf-archive/ax_gcc_func_attribute.html

> Copyright (c) 2013 Gabriele Svelto <gabriele.svelto@gmail.com>
>
> Copying and distribution of this file, with or without modification, are
> permitted in any medium without royalty provided the copyright notice
> and this notice are preserved.  This file is offered as-is, without any
> warranty.


### test images

#### http://public-domain-photos.com/

The following test images in "image/" directory came from PUBLIC-DOMAIN-PHOTOS.com.

- images/egret.jpg

    author: Jon Sullivan
    url: http://public-domain-photos.com/animals/egret-4.htm

- images/snake.jpg

    author: Jon Sullivan
    url: http://public-domain-photos.com/animals/snake-4.htm

These are in the [public domain](http://creativecommons.org/licenses/publicdomain/).


#### vimperator3.png (mascot of vimperator)

images/vimperator3.png is in the public domain.

    author: @k_wizard
    url: http://quadrantem.com/~k_wizard/vimprtan/


## References

### ImageMagick

We are greatly inspired by the quality of ImageMagick and added some resampling filters to
img2sixel in reference to the line-up of filters of MagickCore's resize.c.

    http://www.imagemagick.org/api/MagickCore/resize_8c_source.html


## Similar software

- [ppmtosixel (netpbm)](http://netpbm.sourceforge.net/)

  You can get SIXEL graphics using [ppmtosixel](http://netpbm.sourceforge.net/doc/ppmtosixel.html)
  or [pbmtoln03](http://netpbm.sourceforge.net/doc/ppmtosixel.html).


- [kmiya's sixel](http://nanno.dip.jp/softlib/man/rlogin/sixel.tar.gz)

  libgd based SIXEL converter


- [PySixel](https://pypi.python.org/pypi/PySixel)

  Python implementation of SIXEL converter


- [monosixel in arakiken's tw](https://bitbucket.org/arakiken/tw/branch/sixel)

  A monochrome SIXEL converter


- [sdump](https://github.com/uobikiemukot/sdump)

  SIXEL image dumper. It also provides a PDF viewer and a pager for multiple images.


- [xpr(x11-apps)](ftp://ftp.x.org/pub/unsupported/programs/xpr/)

  xpr(1) can convert a xwd(X window dump) format image into a sixel
  image with '-device ln03' or '-device la100' option.
  But now it is not maintained. It looks broken.


## Other software supporting SIXEL

- [GNUPLOT](http://www.gnuplot.info/)

  Recent version of GNUPLOT supports new terminal driver "sixel".

  ![GNUPLOT](https://raw.githubusercontent.com/saitoha/libsixel/data/data/gnuplot.png)


- [ghostscript](http://www.ghostscript.com/)

  You can emit SIXEL images with LN03 / LN50 / LA75 driver.

  example:

  ```
    $ gs -q -r100x -dBATCH -dNOPAUSE -sDEVICE=ln03 -sOutputFile=- tiger.eps
  ```

  ![GhostScript](https://raw.githubusercontent.com/saitoha/libsixel/data/data/gs.png)


- [ZX81 Emulator](http://rullf2.xs4all.nl/sg/zx81ce.html)

  A ZX81 emulator producing Sixel Image Files

  ![ZX81](https://raw.githubusercontent.com/saitoha/libsixel/data/data/zx81.png)


- [qrc](https://github.com/fumiyas/qrc)

  QR code generator for terminals (ASCII Art, Sixel)

  ![qrc](https://github.com/fumiyas/qrc/blob/master/qrc-demo.png)


- [hiptext](https://github.com/jart/hiptext)

  SIXEL format is supported by -sixel2, -sixel16 or -sixel256 option.

  ![hiptext](https://camo.githubusercontent.com/fc973ffb20a7ff85969df03fd579da2845e62e68/68747470733a2f2f662e636c6f75642e6769746875622e636f6d2f6173736574732f313136323733392f323233393832362f39303361653765382d396335622d313165332d383462362d3539626261346661336430342e706e67)


- [sixelslide](https://github.com/syuu1228/sixelslide)

  Freestanding slideviewer using sixel graphics.
  Currently runs on QEMU(i386), without any filesystem or network.

  cf. http://www.slideshare.net/syuu1228/presentation-on-your-terminal


- ![PGPLOT](http://www.astro.caltech.edu/~tjp/pgplot/)


- [SIXEL to PostScript converter](http://t.co/zTC7LhRbBc)


- [SIXEL image viewer(written in javascript)](http://rullf2.xs4all.nl/sg/sg.html)


- [SixelGraphics.jl(written in Julia)](https://github.com/olofsen/SixelGraphics.jl)


