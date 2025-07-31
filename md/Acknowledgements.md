This software derives from the following implementations.

### sixel 2014-3-2

src/tosixel.c, src/fromsixel.c, and some part of converters/loader.c are
derived from kmiya's "*sixel*" original version (2014-3-2)

  Package: https://nanno.dip.jp/softlib/man/rlogin/sixel.tar.gz

  Unofficial repo: https://github.com/saitoha/sixel

This work is written by kmiya@ culti. He distributes it under very permissive license.

The original license text(in Japanese) is:

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


### stbi-2.12

This software includes `stb-image-v2.12` (`stb_image.h`),
public domain JPEG/PNG reader.

https://github.com/nothings/stb

> LICENSE
>
> This software is in the public domain. Where that dedication is not
> recognized, you are granted a perpetual, irrevocable license to copy,
> distribute, and modify this file as you see fit.


### stbiw-1.02
This software includes `stb-image-write-v1.02` (`stb_image_write.h`),
public domain PNG/BMP/TGA writer.

https://github.com/nothings/stb

> LICENSE
>
> This software is in the public domain. Where that dedication is not
> recognized, you are granted a perpetual, irrevocable license to copy,
> distribute, and modify this file as you see fit.


### pnmquant.c (netpbm library)

The implementation of median cut algorithm for color quantization in quant.c
is imported from `pnmcolormap` included in `netpbm library`.

https://netpbm.sourceforge.net/

`pnmcolormap` was derived from `ppmquant`, originally written by Jef Poskanzer.

> Copyright (C) 1989, 1991 by Jef Poskanzer.
> Copyright (C) 2001 by Bryan Henderson.
>
> Permission to use, copy, modify, and distribute this software and its
> documentation for any purpose and without fee is hereby granted, provided
> that the above copyright notice appear in all copies and that both that
> copyright notice and this permission notice appear in supporting
> documentation.  This software is provided "as is" without express or
> implied warranty.


### loader.h (@uobikiemukot's sdump)

Some parts of converters/loader.c are imported from @uobikiemukot's
[sdump](https://github.com/uobikiemukot/sdump) project

> The MIT License (MIT)
>
> Copyright (c) 2014 haru <uobikiemukot at gmail dot com>
>
> Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:
>
> The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

> THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.


### `ax_gcc_var_attribute` / `ax_gcc_func_attribute`

These are useful m4 macros for detecting some GCC attributes / built-in functions.

<https://www.gnu.org/software/autoconf-archive/ax_gcc_var_attribute.html>
<https://www.gnu.org/software/autoconf-archive/ax_gcc_func_attribute.html>
<https://www.gnu.org/software/autoconf-archive/ax_gcc_builtin.html>

> Copyright (c) 2013 Gabriele Svelto <gabriele.svelto@gmail.com>
>
> Copying and distribution of this file, with or without modification, are
> permitted in any medium without royalty provided the copyright notice
> and this notice are preserved.  This file is offered as-is, without any
> warranty.


### graphics.c (from Xterm pl#310)

The helper function `hls2rgb` in `src/fromsixel.c` is imported from
`graphics.c` in [Xterm pl#310](https://invisible-island.net/xterm/),
originally written by Ross Combs.

> Copyright 2013,2014 by Ross Combs
>
>                         All Rights Reserved
>
> Permission is hereby granted, free of charge, to any person obtaining a
> copy of this software and associated documentation files (the
> "Software"), to deal in the Software without restriction, including
> without limitation the rights to use, copy, modify, merge, publish,
> distribute, sublicense, and/or sell copies of the Software, and to
> permit persons to whom the Software is furnished to do so, subject to
> the following conditions:
>
> The above copyright notice and this permission notice shall be included
> in all copies or substantial portions of the Software.
>
> THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
> OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
> MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
> IN NO EVENT SHALL THE ABOVE LISTED COPYRIGHT HOLDER(S) BE LIABLE FOR ANY
> CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
> TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
> SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
>
> Except as contained in this notice, the name(s) of the above copyright
> holders shall not be used in advertising or otherwise to promote the
> sale, use or other dealings in this Software without prior written
> authorization.


### test images

#### https://public-domain-photos.com/

The following test images in "image/" directory came from PUBLIC-DOMAIN-PHOTOS.com.

- images/egret.jpg

    author: Jon Sullivan
    url: https://public-domain-photos.com/animals/egret-4.htm

- images/snake.jpg

    author: Jon Sullivan
    url: https://public-domain-photos.com/animals/snake-4.htm

These are in the [public domain](https://creativecommons.org/licenses/publicdomain/).


#### vimperator3.png (mascot of vimperator)

images/vimperator3.png is in the public domain.

    author: @k_wizard
    url: https://quadrantem.com/~k_wizard/vimprtan/


#### PngSuite

Images under the directory images/pngsuite/ are imported from
[PngSuite](https://www.schaik.com/pngsuite/) created by Willem van Schaik.

> Permission to use, copy, modify and distribute these images for any
> purpose and without fee is hereby granted.
>
>
>(c) Willem van Schaik, 1996, 2011


## References

### ImageMagick

We are greatly inspired by the quality of ImageMagick and added some resampling filters to
`img2sixel` in reference to the line-up of filters of MagickCore's resize.c.

    https://www.imagemagick.org/api/MagickCore/resize_8c_source.html

