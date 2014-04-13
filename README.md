libsixel
========

## What is this?

This package provides encoder/decoder implementation for DEC SIXEL graphics, and
some converter programs.

![img2sixel](http://zuse.jp/misc/libsixel-1.png)

SIXEL is one of image formats for terminal imaging introduced by DEC VT series.
Its data scheme is represented as a terminal-friendly escape sequence.
So if you want to view a SIXEL image file, all you have to do is "cat" it to your terminal.

## Build and install

```
$ ./configure
$ make
# make install
```

To build source package:

```
$ make package
```


## Terminal requirements

If you want to view a SIXEL image, you have to get a terminal which support sixel graphics.

Now SIXEL feature is supported by the following terminals.

- VT240

- VT241

- VT330

- VT340

- VT382

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

- DECterm

- Kermit

- WRQ Reflection

- ZSTEM


## Usage of command line tools

### img2sixel

```
Usage: img2sixel [Options] imagefiles
       img2sixel [Options] < imagefile

Options:
-p, --colors       specify number of colors to reduce the image to
-m, --mapfile      transform image colors to match this set of colorsspecify map
-e, --monochrome   output monochrome sixel image
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
```

Convert a sixel file into a png image file

```
$ sixel2png < egret.sixel > egret.png
```

## Similar software

- [ppmtosixel (netpbm)](http://netpbm.sourceforge.net/)

  You can get SIXEL graphics using [ppmtosixel](http://netpbm.sourceforge.net/doc/ppmtosixel.html) or [pbmtoln03](http://netpbm.sourceforge.net/doc/ppmtosixel.html).


- [kmiya's sixel](http://nanno.dip.jp/softlib/man/rlogin/sixel.tar.gz)

  libgd based SIXEL converter


- [PySixel](https://pypi.python.org/pypi/PySixel)

  Python implementation of SIXEL converter


## Other software supporting SIXEL

- [GNUPLOT](http://www.gnuplot.info/)

  Recent version of GNUPLOT supports new terminal driver "sixel".

  ![GNUPLOT](http://zuse.jp/misc/gnuplot.png)


- [ghostscript](http://www.ghostscript.com/)

  You can emit SIXEL images with LN03 / LN50 / LA75 driver.

  example:

  ```
    $ gs -q -r100x -dBATCH -dNOPAUSE -sDEVICE=ln03 -sOutputFile=- tiger.eps
  ```

  ![GhostScript](http://zuse.jp/misc/gs.png)


- ![PGPLOT](http://www.astro.caltech.edu/~tjp/pgplot/)


## Color image quantization quality comparison


- ppmtosixel (netpbm)

    $ jpegtopnm images/snake.jpg | pnmquant 16 | ppmtosixel

  ![ppmtosixel](http://zuse.jp/misc/q_ppmtosixel.png)


- ppmtosixel with Floyd–Steinberg dithering (netpbm)

    $ jpegtopnm images/snake.jpg | pnmquant 16 -floyd | ppmtosixel

  ![ppmtosixel](http://zuse.jp/misc/q_ppmtosixel2.png)


- kmiya's sixel

    $ sixel -p16 images/snake.jpg

  ![kmiya's sixel](http://zuse.jp/misc/q_sixel.png)


- PySixel (sixelconv command)

    $ sixelconv -n16 images/snake.jpg

  ![PySixel](http://zuse.jp/misc/q_sixelconv.png)


- libsixel (img2sixel command)

    $ img2sixel -p16 images/snake.jpg

  ![PySixel](http://zuse.jp/misc/q_libsixel.png)


## Support

This software is provided "as is" without express or implied warranty.
The main support channel for this software is the github issue tracker:
    
  [https://github.com/saitoha/libsixel/issues](https://github.com/saitoha/libsixel/issues)


Please post your issue if you have any problems, questions or suggestions.


## License

The MIT License (MIT)

Copyright (c) 2014 Hayaki Saito

Permission is hereby granted, free of charge, to any person obtaining a copy of
this software and associated documentation files (the "Software"), to deal in
the Software without restriction, including without limitation the rights to
use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of
the Software, and to permit persons to whom the Software is furnished to do so,
subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS
FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR
COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER
IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.


## Thanks

This software derives from the following implementations.

### sixel 2014-3-2

tosixel.c and fromsixel.c are derived from kmiya's "*sixel*" original version (2014-3-2)

http://nanno.dip.jp/softlib/man/rlogin/sixel.tar.gz

It is written by kmiya@culti.

He distributes it under very permissive license which permits
useing, copying, modification, redistribution, and all other
public activities without any restrictions.

He declares this is compatible with MIT/BSD/GPL.


### stbi-1.33

This software includes *stbi-1.33* (stb_image.c),
public domain JPEG/PNG reader.

http://nothings.org/stb_image.c


### stbiw-0.92

This software includes *stbiw-0.92* (stb_image_write.h),
public domain PNG/BMP/TGA writer.

http://nothings.org/stb/stb_image_write.h


### pnmquant.c (netpbm library)

The implementation of median cut algorithm for color quantization in quant.c
is imported from *pnmcolormap* included in *netpbm library*.

http://netpbm.sourceforge.net/

*pnmcolormap* was derived from *ppmquant*, originally by Jef Poskanzer.


Copyright (C) 1989, 1991 by Jef Poskanzer.
Copyright (C) 2001 by Bryan Henderson.

Permission to use, copy, modify, and distribute this software and its
documentation for any purpose and without fee is hereby granted, provided
that the above copyright notice appear in all copies and that both that
copyright notice and this permission notice appear in supporting
documentation.  This software is provided "as is" without express or
implied warranty.
 
### test images (egret.jpg / snake.jpg)

Test images in "image/" directory came from PUBLIC-DOMAIN-PHOTOS.com

http://public-domain-photos.com/

- images/egret.jpg

    author: Jon Sullivan
    url: http://public-domain-photos.com/animals/egret-4.htm

- images/snake.jpg

    author: Jon Sullivan
    url: http://public-domain-photos.com/animals/snake-4.htm

