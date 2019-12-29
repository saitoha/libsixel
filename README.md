libsixel
========

[![Build Status](https://travis-ci.org/saitoha/libsixel.svg?branch=master)](https://travis-ci.org/saitoha/libsixel)
[![Coverage Status](https://coveralls.io/repos/saitoha/libsixel/badge.png?branch=master)](https://coveralls.io/r/saitoha/libsixel?branch=master)

## What is this?

This package provides encoder/decoder implementation for DEC SIXEL graphics, and
some converter programs.

![img2sixel](https://raw.githubusercontent.com/saitoha/libsixel/data/data/libsixel-1.png)(https://youtu.be/0SasrQ7pnbA)

SIXEL is one of image formats for printer and terminal imaging introduced by
Digital Equipment Corp. (DEC).
Its data scheme is represented as a terminal-friendly escape sequence.
So if you want to view a SIXEL image file, all you have to do is "cat" it to your terminal.

On 80's real hardware terminals, it tooks unbearable long waiting times to display images.

[![vt330sixel](https://raw.githubusercontent.com/saitoha/libsixel/data/data/vt330sixel.png)](https://youtu.be/0SasrQ7pnbA)

But nowdays, with high-speed CPU and broadband network, we got the chance to develop a new scope of SIXELs.

## SIXEL Animation

`img2sixel(1)` can decode GIF animation.

  ![Animation](https://raw.githubusercontent.com/saitoha/libsixel/data/data/sixel.gif)


## Related projects

### Video streaming

Now Youtube video streaming is available over SIXEL protocol by [FFmpeg-SIXEL](https://github.com/saitoha/FFmpeg-SIXEL) project.

  [![FFmpeg-SIXEL](https://raw.githubusercontent.com/saitoha/libsixel/data/data/ffmpeg.png)](http://youtu.be/hqMh47lYHlc)

Above demo only uses 16 color registers.

### SDL integration: Gaming, Virtualization, ...etc.

[SDL1.2-SIXEL](https://github.com/saitoha/SDL1.2-SIXEL) project makes enable you to operate various GUI applications on the terminal.

You can play "`The Battle for Wesnoth`" over SIXEL protocol.

  [![SDL1.2-SIXEL WESNOTH](https://raw.githubusercontent.com/saitoha/libsixel/data/data/wesnoth.png)](http://youtu.be/aMUkN7TSct4)

You can run QEMU on SIXEL terminals.

  [![SDL1.2-SIXEL QEMU](https://raw.githubusercontent.com/saitoha/libsixel/data/data/qemu.png)](http://youtu.be/X6M5tgNjEuQ)

[SDL1.2-SIXEL](https://github.com/saitoha/SDL1.2-SIXEL) can collaborate with [XSDL-SIXEL](https://github.com/saitoha/xserver-xsdl-sixel).

  [![SDL1.2-SIXEL XSDL](https://raw.githubusercontent.com/saitoha/libsixel/data/data/xsdl.png)](http://youtu.be/UOTMGdUPYRo)

Furthermore some information of SIXEL-ready SDL applications are reported.

- [NetSurf](https://www.reddit.com/r/commandline/comments/4qyb90/netsurf_a_graphical_browser_on_xterm_using_sixel/)
  ([screenshot](http://imgur.com/a/Y6xH6))

- [Green PDF Viewer](https://www.reddit.com/r/commandline/comments/4oldf5/view_pdfs_in_terminal_requires_nixos_latest_git/)
  ([screenshot](https://m.reddit.com/r/commandline/comments/4oldf5/view_pdfs_in_terminal_requires_nixos_latest_git/))

- [DOOM](https://www.libsdl.org/projects/doom/)
  ([tweet](https://twitter.com/rattcv/status/775213402130046977))

- [firesdl](https://github.com/klange/firesdl)
  ([movie](https://www.youtube.com/watch?v=XubH2W39Xtc))


### Langage Bindings

#### [libsixel-python](https://pypi.python.org/pypi/libsixel-python/0.4.0)

  [converter.py](https://github.com/saitoha/libsixel/blob/master/examples/python/converter.py) example depends on it.

#### [mruby-sixel](https://github.com/kjunichi/mruby-sixel)

  Used by [mruby-webcam](https://github.com/kjunichi/mruby-webcam).

#### [libsixel-p6](https://github.com/timo/libsixel-p6)

  A [perl6](https://perl6.org/) bindings for libsixel

#### [sixel-sys](https://github.com/AdnoC/sixel-sys)

  [Rust](https://www.rust-lang.org/) FFI bindings for libsixel

#### [sixel-rs](https://github.com/AdnoC/sixel-rs)

  A safe [Rust](https://www.rust-lang.org/) wrapper for libsixel


### W3M integration

`img2sixel(1)` can be integrated with [Debian's w3m](https://tracker.debian.org/pkg/w3m)(maintained by [Tatsuya Kinoshita](https://github.com/tats)) that includes patches for *-sixel* option derived from [Arakiken's w3m fork(remoteimg branch)](https://bitbucket.org/arakiken/w3m/branch/remoteimg).

  ![w3m-sixel](https://raw.githubusercontent.com/saitoha/libsixel/data/data/w3m-sixel.png)


@uobikiemukot's [sdump](https://github.com/uobikiemukot/sdump) project selected another approach.
He wrote a w3mimgdisplay compatible program [yaimg-sixel](https://github.com/uobikiemukot/sdump/tree/master/yaimg-sixel).
It also works with [ranger](https://github.com/hut/ranger).

  ![w3m-yaimg-sixel](https://raw.githubusercontent.com/saitoha/libsixel/data/data/w3m-yaimg-sixel.jpg)


### X11 on SIXEL terminals

[Xsixel](https://github.com/saitoha/xserver-sixel) is a kdrive server implementation for SIXEL terminals.

  ![Xsixel](https://raw.githubusercontent.com/saitoha/libsixel/data/data/xsixel.png)

  ![Xsixel Blue Print](https://raw.githubusercontent.com/saitoha/libsixel/data/data/HowToBuildTerminalGUI.png)


### GNU Screen integration

[Arakiken's GNU Screen fork(sixel branch)](https://bitbucket.org/arakiken/screen/branch/sixel)
works with SIXEL-supported applications including above products.
This project is now in progress.
GUI flavored SIXEL applications will integrated with existing terminal applications on it.

  ![w3m-sixel-screen](https://raw.githubusercontent.com/saitoha/libsixel/data/data/w3m-sixel-screen.png)

  ![sixel-screen](https://raw.githubusercontent.com/saitoha/libsixel/data/data/arakikens-screen.jpg)

  ![xsixel-screen](https://raw.githubusercontent.com/saitoha/libsixel/data/data/xsixel-on-screen.png)

See also on [youtube](http://youtu.be/QQAqe32VkFg).

### Twitter client integration

Some NetBSD/OpenBSD users are doing amazing challenges.

#### [arakiken's mikutterm(mikutterm-sixel)](https://bitbucket.org/arakiken/mikutterm/branch/sixel)

  Now [mikutter](http://mikutter.hachune.net/) + [mikutterm](https://bitbucket.org/arakiken/mikutterm) works with libsixel inline-image extension.

  SIXEL works with old powerless machines such as

  [NetBSD/luna68k](http://wiki.netbsd.org/ports/luna68k/) (here is OMRON LUNA-II):

  ![mikutterm-netbsd-luna68k](https://raw.githubusercontent.com/saitoha/libsixel/data/data/mikutterm-netbsd-luna68k.jpg)

  [NetBSD/hp300](http://wiki.netbsd.org/ports/hp300/) (here is HP9000/425e):

  ![mikutterm-netbsd-hp9000](https://raw.githubusercontent.com/saitoha/libsixel/data/data/mikutterm-netbsd-hp9000.jpg)


#### [arakiken's tw(tw-sixel)](https://bitbucket.org/arakiken/tw/branch/sixel)

  [arakiken's tw(tw-sixel)](https://bitbucket.org/arakiken/tw/branch/sixel) works with libsixel inline-image extension.

  SIXEL works with old powerless machines such as [OpenBSD/luna88k](http://www.openbsd.org/luna88k.html) (here is OMRON LUNA-88K2 MC88100@33MHz):

  ![mikutterm-netbsd-hp9000](https://raw.githubusercontent.com/saitoha/libsixel/data/data/tw-openbsd-luna88k.jpg)


#### [sayaka-chan](https://github.com/isaki68k/sayaka/)

  [sayaka-chan](https://github.com/isaki68k/sayaka/)(PHP version) works with libsixel inline-image extension.

  SIXEL works with old powerless machines such as [NetBSD/x68k](http://wiki.netbsd.org/ports/x68k/) (here is SHARP X68030 with 060turbo):

  ![sayaka-chan](https://raw.githubusercontent.com/saitoha/libsixel/data/data/sayaka-netbsd-x68k.jpg)

  SIXEL works even in-kernel console. [@isaki68k](https://github.com/isaki68k) wrote
  [a patch for ite(4)](https://github.com/isaki68k/misc/blob/master/NetBSD/patch/x68k-ite-sixel.diff).

  ![ite(4)](https://raw.githubusercontent.com/saitoha/libsixel/data/data/ite.png)


### Other

#### [sixelSPAD](https://github.com/nilqed/sixelSPAD)

  [screenshot](https://nilqed.github.io/drawfe/)

  Includes 2 commands [fricas2sixel](https://github.com/nilqed/sixelSPAD/blob/master/bin/fricas2sixel)
  and [latex2sixel](https://github.com/nilqed/sixelSPAD/blob/master/bin/latex2sixel).

  ![latex2sixel](https://raw.githubusercontent.com/saitoha/libsixel/data/data/latex2sixel.jpg)

#### [Neofetch](https://github.com/dylanaraps/neofetch)

  Now `sixel` backend is implemented.
  See https://github.com/dylanaraps/neofetch/wiki/Image-Backends#sixel

  ![neofetch](https://raw.githubusercontent.com/saitoha/libsixel/data/data/neofetch.png)

#### [termplay](https://github.com/jD91mZM2/termplay)

  Depends on [sixel-sys](https://github.com/AdnoC/sixel-sys), `--converter=sixel` option is supported.

  [![termplay](https://github.com/saitoha/libsixel/blob/data/data/termplay.png)](https://youtu.be/sOHU1b-Ih90)

#### [sixelPreviewer](https://github.com/mikoto2000/sixelPreviewer)

  Simple scripts and development environment for realtime edit-previewing for dot, svg, markdown, ...etc.
  [![sixelPreviewer](https://raw.githubusercontent.com/saitoha/libsixel/data/data/sixelpreviewer.png)](https://youtu.be/iPzHWPGWHV4)

#### [sdump](https://github.com/uobikiemukot/sdump)

  A sixel image dumper, provides pdf previewer.

### [RetroArch](https://github.com/libretro/RetroArch)

  SIXEL video driver is provided if you build it with `--enable-sixel` option.
  ([screenshot](https://imgur.com/lf3bh2S))
  
  
## Highlighted features

### Improved compression

Former sixel encoders(such as [ppmtosixel](http://netpbm.sourceforge.net/doc/ppmtosixel.html)) are mainly designed for dot-matrix printers.
They minimize the amount of printer-head movement distance.
But nowadays this method did not represent the best performance for displaying sixel data on terminal emulators.
SIXEL data for terminals were found in 80's Usenet, but the technology of how to create them seems to be lost.
[kmiya's sixel](http://nanno.dip.jp/softlib/man/rlogin/sixel.tar.gz) introduces the encoding method which is re-designed
for terminal emulators to optimize the overhead of transporting SIXEL with keeping compatibility with former SIXEL terminal.
Now libsixel and ImageMagick's sixel coder follow it.

@arakiken, known as the maintainer of mlterm, describes about the way to generate high quality SIXEL, which is adopted by libsixel
([http://mlterm.sourceforge.net/libsixel.pdf](http://mlterm.sourceforge.net/libsixel.pdf), in Japanese).


### High quality quantization

`img2sixel(1)` supports color image quantization. It works well even if few number of colors are allowed.


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

- mlterm

  [http://mlterm.sourceforge.net/](http://mlterm.sourceforge.net/)

  Works on each of X, WIN32 GDI, framebuffer, Android, Cocoa version.

- XTerm (compiled with `--enable-sixel-graphics` option)

  [http://invisible-island.net/xterm/](http://invisible-island.net/xterm/)

  You should launch xterm with "`-ti vt340`" option.
  The SIXEL palette is limited to a maximum of 16 colors.
  To avoid this limitation, Try

```sh
$ echo "XTerm*decTerminalID: vt340" >> $HOME/.Xresources
$ echo "XTerm*numColorRegisters: 256" >>  $HOME/.Xresources
$ xrdb $HOME/.Xresources
$ xterm
```

  or

```sh
$ xterm -xrm "XTerm*decTerminalID: vt340" -xrm "XTerm*numColorRegisters: 256"
```

- yaft

  [https://github.com/uobikiemukot/yaft](https://github.com/uobikiemukot/yaft)

- recterm (ttyrec to GIF converter)

  [https://github.com/uobikiemukot/recterm](https://github.com/uobikiemukot/recterm)

- seq2gif (ttyrec to GIF converter)

  [https://github.com/saitoha/seq2gif](https://github.com/saitoha/seq2gif)

- Mintty (>= 2.6.0)

  [https://mintty.github.io/](https://mintty.github.io/)

- cancer
  [https://github.com/meh/cancer/](https://github.com/meh/cancer)

- MacTerm
  [https://github.com/kmgrant/macterm](https://github.com/kmgrant/macterm)

- wezterm
  [https://github.com/wez/wezterm](https://github.com/wez/wezterm)

- aminal
  [https://github.com/liamg/aminal](https://github.com/liamg/aminal)

- iTerm2 (>= 3.0.0)
  [https://gitlab.com/gnachman/iterm2](https://gitlab.com/gnachman/iterm2)

- st-sixel
  [https://github.com/galatolofederico/st-sixel](https://github.com/galatolofederico/st-sixel)

- DomTerm
  [https://github.com/PerBothner/DomTerm](https://github.com/PerBothner/DomTerm)

- yaft-cocoa
  [https://github.com/uobikiemukot/yaft-cocoa](https://github.com/uobikiemukot/yaft-cocoa)

- toyterm
  [https://github.com/algon-320/toyterm](https://github.com/algon-320/toyterm)


## Install

### Using package managers

You can install libsixel via the following package systems.

- [FreeBSD ports](http://portsmon.freebsd.org/portoverview.py?category=graphics&portname=libsixel)
- [DPorts](https://github.com/DragonFlyBSD/DPorts/tree/master/graphics/libsixel)
- [pkgsrc](http://cvsweb.netbsd.org/bsdweb.cgi/pkgsrc/graphics/libsixel/)
- [Homebrew](https://formulae.brew.sh/formula/libsixel)
- [yacp](https://github.com/fd00/yacp/tree/master/libsixel)
- [Debian](https://packages.debian.org/search?searchon=names&keywords=libsixel)
- [AUR](https://aur.archlinux.org/packages/libsixel-git/)
- [Portage](http://packages.gentoo.org/package/media-libs/libsixel)
- [Ubuntu](https://launchpad.net/ubuntu/+source/libsixel)
- [NixOS](https://github.com/NixOS/nixpkgs/blob/master/pkgs/development/libraries/libsixel/default.nix)
- [OpenBSD Ports](http://openports.se/graphics/libsixel)
- [Fedora Copr](https://copr.fedorainfracloud.org/coprs/saahriktu/libsixel/)
- [SlackBuilds](https://slackbuilds.org/repository/14.2/libraries/libsixel/)

### Build from source package

```
$ ./configure
$ make
# make install
```

#### Build with optional packages

You can configure with the following options

```
--with-libcurl            build with libcurl (default: auto)
--with-gd                 build with libgd (default: no)
--with-gdk-pixbuf2        build with gdk-pixbuf2 (default: no)
--with-jpeg               build with libjpeg (default: auto)
--with-png                build with libpng (default: auto)
--with-pkgconfigdir       specify pkgconfig dir (default is libdir/pkgconfig)
--with-bashcompletiondir  specify bashcompletion.d
--with-zshcompletiondir   specify zshcompletion.d
--enable-python           Python interface (default: yes)
--enable-debug            Use debug macro and specific CFLAGS
--enable-gcov             Use gcov
--enable-tests            Build tests
```

For more information, see "./configure --help".


##### Cross compiling with MinGW

You can build a windows binary in cross-build environment.

```
$ CC=i686-w64-mingw32-gcc cross_compile=yes ./configure --host=i686-w64-mingw32
$ make
```

## Usage of command line tools

### img2sixel

```
Usage: img2sixel [Options] imagefiles
       img2sixel [Options] < imagefile

Options:
-o, --outfile              specify output file name.
                           (default:stdout)
-7, --7bit-mode            generate a sixel image for 7bit
                           terminals or printers (default)
-8, --8bit-mode            generate a sixel image for 8bit
                           terminals or printers
-R, --gri-limit            limit arguments of DECGRI('!') to 255
-p COLORS, --colors=COLORS specify number of colors to reduce
                           the image to (default=256)
-m FILE, --mapfile=FILE    transform image colors to match this
                           set of colorsspecify map
-e, --monochrome           output monochrome sixel image
                           this option assumes the terminal
                           background color is black
-k, --insecure             allow to connect to SSL sites without
                           certs(enabled only when configured
                           with --with-libcurl)
-i, --invert               assume the terminal background color
                           is white, make sense only when -e
                           option is given
-I, --high-color           output 15bpp sixel image
-u, --use-macro            use DECDMAC and DEVINVM sequences to
                           optimize GIF animation rendering
-n MACRONO, --macro-number=MACRONO
                           specify an number argument for
                           DECDMAC and make terminal memorize
                           SIXEL image. No image is shown if
                           this option is specified
-C COMPLEXIONSCORE, --complexion-score=COMPLEXIONSCORE
                           specify an number argument for the
                           score of complexion correction.
                           COMPLEXIONSCORE must be 1 or more.
-g, --ignore-delay         render GIF animation without delay
-S, --static               render animated GIF as a static image
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
                             a_dither -> positionally stable
                                         arithmetic dither
                             x_dither -> positionally stable
                                         arithmetic xor based dither
-f FINDTYPE, --find-largest=FINDTYPE
                           choose method for finding the largest
                           dimension of median cut boxes for
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
                             auto      -> choose selecting
                                          method automatically
                                          (default)
                             center    -> choose the center of
                                          the box
                             average    -> calculate the color
                                          average into the box
                             histogram -> similar with average
                                          but considers color
                                          histogram
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
                             low  -> low quality and high
                                     speed mode
                             high -> high quality and low
                                     speed mode
                             full -> full quality and careful
                                     speed mode
-l LOOPMODE, --loop-control=LOOPMODE
                           select loop control mode for GIF
                           animation.
                             auto    -> honor the setting of
                                        GIF header (default)
                             force   -> always enable loop
                             disable -> always disable loop
-t PALETTETYPE, --palette-type=PALETTETYPE
                           select palette color space type
                             auto -> choose palette type
                                     automatically (default)
                             hls  -> use HLS color space
                             rgb  -> use RGB color space
-b BUILTINPALETTE, --builtin-palette=BUILTINPALETTE
                           select built-in palette type
                             xterm16    -> X default 16 color map
                             xterm256   -> X default 256 color map
                             vt340mono  -> VT340 monochrome map
                             vt340color -> VT340 color map
                             gray1      -> 1bit grayscale map
                             gray2      -> 2bit grayscale map
                             gray4      -> 4bit grayscale map
                             gray8      -> 8bit grayscale map
-E ENCODEPOLICY, --encode-policy=ENCODEPOLICY
                           select encoding policy
                             auto -> choose encoding policy
                                     automatically (default)
                             fast -> encode as fast as possible
                             size -> encode to as small sixel
                                     sequence as possible
-B BGCOLOR, --bgcolor=BGCOLOR
                           specify background color
                           BGCOLOR is represented by the
                           following syntax
                             #rgb
                             #rrggbb
                             #rrrgggbbb
                             #rrrrggggbbbb
                             rgb:r/g/b
                             rgb:rr/gg/bb
                             rgb:rrr/ggg/bbb
                             rgb:rrrr/gggg/bbbb
-P, --penetrate            penetrate GNU Screen using DCS
                           pass-through sequence
-D, --pipe-mode            [[deprecated]] read source images from
                           stdin continuously
-v, --verbose              show debugging info
-V, --version              show version and license info
-H, --help                 show this help

Environment variables:
SIXEL_BGCOLOR              specify background color.
                           overrided by -B(--bgcolor) option.
                           represented by the following
                           syntax:
                             #rgb
                             #rrggbb
                             #rrrgggbbb
                             #rrrrggggbbbb
                             rgb:r/g/b
                             rgb:rr/gg/bb
                             rgb:rrr/ggg/bbb
                             rgb:rrrr/gggg/bbbb

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

## The high-level conversion API

The high-livel API provides File-to-File conversion features.


### Encoder

The sixel encoder object and related functions provides almost same features as `img2sixel`.

```C
/* create encoder object */
SIXELAPI SIXELSTATUS
sixel_encoder_new(
    sixel_encoder_t     /* out */ **ppencoder, /* encoder object to be created */
    sixel_allocator_t   /* in */  *allocator); /* allocator, null if you use
                                                  default allocator */

/* increase reference count of encoder object (thread-unsafe) */
SIXELAPI void
sixel_encoder_ref(sixel_encoder_t /* in */ *encoder);

/* decrease reference count of encoder object (thread-unsafe) */
SIXELAPI void
sixel_encoder_unref(sixel_encoder_t /* in */ *encoder);

/* set cancel state flag to encoder object */
SIXELAPI SIXELSTATUS
sixel_encoder_set_cancel_flag(
    sixel_encoder_t /* in */ *encoder,
    int             /* in */ *cancel_flag);

/* set an option flag to encoder object */
SIXELAPI SIXELSTATUS
sixel_encoder_setopt(
    sixel_encoder_t /* in */ *encoder,
    int             /* in */ arg,
    char const      /* in */ *optarg);

/* load source data from specified file and encode it to SIXEL format */
SIXELAPI SIXELSTATUS
sixel_encoder_encode(
    sixel_encoder_t /* in */ *encoder,
    char const      /* in */ *filename);
```

### Decoder

The sixel decoder object and related functions provides almost same features as `sixel2png`.

```C
/* create decoder object */
SIXELAPI SIXELSTATUS
sixel_decoder_new(
    sixel_decoder_t    /* out */ **ppdecoder,  /* decoder object to be created */
    sixel_allocator_t  /* in */  *allocator);  /* allocator, null if you use
                                                  default allocator */

/* increase reference count of decoder object (thread-unsafe) */
SIXELAPI void
sixel_decoder_ref(sixel_decoder_t *decoder);

/* decrease reference count of decoder object (thread-unsafe) */
SIXELAPI void
sixel_decoder_unref(sixel_decoder_t *decoder);

/* set an option flag to decoder object */
SIXELAPI SIXELSTATUS
sixel_decoder_setopt(
    sixel_decoder_t /* in */ *decoder,  /* decoder object */
    int             /* in */ arg,       /* one of SIXEL_OPTFLAG_*** */
    char const      /* in */ *optarg);  /* null or an argument of optflag */

/* load source data from stdin or the file specified with
   SIXEL_OPTFLAG_INPUT flag, and decode it */
SIXELAPI SIXELSTATUS
sixel_decoder_decode(
    sixel_decoder_t /* in */ *decoder);
```


## The low-level conversion API

The low-livel API provides Bytes-to-Bytes conversion features.

The Whole API is described [here](https://github.com/saitoha/libsixel/blob/master/include/sixel.h.in).

### Examples

#### OpenGL

[OpenGL example](https://github.com/saitoha/libsixel/blob/master/examples/opengl/)
suggests how to port your OpenGL application to SIXEL terminal.

  ![opengl example](https://raw.githubusercontent.com/saitoha/libsixel/data/data/example_opengl.gif)


#### Drawing

[Drawing example](https://github.com/saitoha/libsixel/blob/master/examples/drawing/)
suggests how to implement the interaction among SIXEL terminals and pointer devices.

  [![drawing](https://raw.githubusercontent.com/saitoha/libsixel/data/data/drawing.png)](https://youtu.be/2-2FnoZp4Z0)


#### Python

[Python example](https://github.com/saitoha/libsixel/blob/master/examples/python/)
suggests how to convert PIL images into SIXEL using libsixel python interface.


### Bitmap to SIXEL

`sixel_encode` function converts bitmap array into SIXEL format.

```C
/* convert pixels into sixel format and write it to output context */
SIXELAPI SIXELSTATUS
sixel_encode(
    unsigned char  /* in */ *pixels,     /* pixel bytes */
    int            /* in */  width,      /* image width */
    int            /* in */  height,     /* image height */
    int            /* in */  depth,      /* color depth: now unused */
    sixel_dither_t /* in */ *dither,     /* dither context */
    sixel_output_t /* in */ *context);   /* output context */
```
To use this function, you have to initialize two objects,

- `sixel_dither_t` (dithering context object)
- `sixel_output_t` (output context object)

#### Dithering context

Here is a part of APIs for dithering context manipulation.

```C
/* create dither context object */
SIXELAPI SIXELSTATUS
sixel_dither_new(
    sixel_dither_t      /* out */   **ppdither,  /* dither object to be created */
    int                 /* in */    ncolors,     /* required colors */
    sixel_allocator_t   /* in */    *allocator); /* allocator, null if you use
                                                    default allocator */

/* get built-in dither context object */
SIXELAPI sixel_dither_t *
sixel_dither_get(int builtin_dither); /* ID of built-in dither object */

/* destroy dither context object */
SIXELAPI void
sixel_dither_destroy(sixel_dither_t *dither); /* dither context object */

/* increase reference count of dither context object (thread-unsafe) */
SIXELAPI void
sixel_dither_ref(sixel_dither_t *dither); /* dither context object */

/* decrease reference count of dither context object (thread-unsafe) */
SIXELAPI void
sixel_dither_unref(sixel_dither_t *dither); /* dither context object */

/* initialize internal palette from specified pixel buffer */
SIXELAPI SIXELSTATUS
sixel_dither_initialize(
    sixel_dither_t *dither,          /* dither context object */
    unsigned char /* in */ *data,    /* sample image */
    int /* in */ width,              /* image width */
    int /* in */ height,             /* image height */
    int /* in */ pixelformat,        /* one of enum pixelFormat */
    int /* in */ method_for_largest, /* method for finding the largest dimension */
    int /* in */ method_for_rep,     /* method for choosing a color from the box */
    int /* in */ quality_mode);      /* quality of histogram processing */

/* set diffusion type, choose from enum methodForDiffuse */
SIXELAPI void
sixel_dither_set_diffusion_type(
    sixel_dither_t /* in */ *dither,   /* dither context object */
    int /* in */ method_for_diffuse);  /* one of enum methodForDiffuse */

/* get number of palette colors */
SIXELAPI int
sixel_dither_get_num_of_palette_colors(
    sixel_dither_t /* in */ *dither);  /* dither context object */

/* get number of histogram colors */
SIXELAPI int
sixel_dither_get_num_of_histogram_colors(
    sixel_dither_t /* in */ *dither);  /* dither context object */

/* get palette */
SIXELAPI unsigned char *
sixel_dither_get_palette(
    sixel_dither_t /* in */ *dither);  /* dither context object */

/* set palette */
SIXELAPI void
sixel_dither_set_palette(
    sixel_dither_t /* in */ *dither,   /* dither context object */
    unsigned char  /* in */ *palette);

SIXELAPI void
sixel_dither_set_complexion_score(
    sixel_dither_t /* in */ *dither,   /* dither context object */
    int            /* in */ score);    /* complexion score (>= 1) */

SIXELAPI void
sixel_dither_set_body_only(
    sixel_dither_t /* in */ *dither,   /* dither context object */
    int            /* in */ bodyonly); /* 0: output palette section(default)
                                          1: do not output palette section */
SIXELAPI void
sixel_dither_set_optimize_palette(
    sixel_dither_t /* in */ *dither,   /* dither context object */
    int            /* in */ do_opt);   /* 0: optimize palette size
                                          1: don't optimize palette size */
/* set pixelformat */
SIXELAPI void
sixel_dither_set_pixelformat(
    sixel_dither_t /* in */ *dither,      /* dither context object */
    int            /* in */ pixelformat); /* one of enum pixelFormat */

/* set transparent */
SIXELAPI void
sixel_dither_set_transparent(
    sixel_dither_t /* in */ *dither,      /* dither context object */
    int            /* in */ transparent); /* transparent color index */
```

#### Output context

Here is a part of APIs for output context manipulation.

```C
/* create output context object */
SIXELAPI SIXELSTATUS
sixel_output_new(
    sixel_output_t          /* out */ **output,     /* output object to be created */
    sixel_write_function    /* in */  fn_write,     /* callback for output sixel */
    void                    /* in */ *priv,         /* private data given as
                                                       3rd argument of fn_write */
    sixel_allocator_t       /* in */  *allocator);  /* allocator, null if you use
                                                       default allocator */

/* destroy output context object */
SIXELAPI void
sixel_output_destroy(sixel_output_t /* in */ *output); /* output context */

/* increase reference count of output context object (thread-unsafe) */
SIXELAPI void
sixel_output_ref(sixel_output_t /* in */ *output);     /* output context */

/* decrease reference count of output context object (thread-unsafe) */
SIXELAPI void
sixel_output_unref(sixel_output_t /* in */ *output);   /* output context */

/* set 8bit output mode which indicates whether it uses C1 control characters */
SIXELAPI int
sixel_output_get_8bit_availability(
    sixel_output_t /* in */ *output);   /* output context */

/* get 8bit output mode state */
SIXELAPI void
sixel_output_set_8bit_availability(
    sixel_output_t /* in */ *output,       /* output context */
    int            /* in */ availability); /* 0: do not use 8bit characters
                                              1: use 8bit characters */

/* set GNU Screen penetration feature enable or disable */
SIXELAPI void
sixel_output_set_penetrate_multiplexer(
    sixel_output_t /* in */ *output,    /* output context */
    int            /* in */ penetrate); /* 0: penetrate GNU Screen
                                           1: do not penetrate GNU Screen */

/* set whether we skip DCS envelope */
SIXELAPI void
sixel_output_set_skip_dcs_envelope(
    sixel_output_t /* in */ *output,   /* output context */
    int            /* in */ skip);     /* 0: output DCS envelope
                                          1: do not output DCS envelope */

SIXELAPI void
sixel_output_set_palette_type(
    sixel_output_t /* in */ *output,      /* output context */
    int            /* in */ palettetype); /* PALETTETYPE_RGB: RGB palette
                                             PALETTETYPE_HLS: HLS palette */

SIXELAPI void
sixel_output_set_encode_policy(
    sixel_output_t /* in */ *output,    /* output context */
    int            /* in */ encode_policy);
```

### SIXEL to indexed bitmap

`sixel_decode` function converts SIXEL into indexed bitmap bytes with its palette.

```
/* convert sixel data into indexed pixel bytes and palette data */
SIXELAPI SIXELSTATUS
sixel_decode_raw(
    unsigned char       /* in */  *p,           /* sixel bytes */
    int                 /* in */  len,          /* size of sixel bytes */
    unsigned char       /* out */ **pixels,     /* decoded pixels */
    int                 /* out */ *pwidth,      /* image width */
    int                 /* out */ *pheight,     /* image height */
    unsigned char       /* out */ **palette,    /* ARGB palette */
    int                 /* out */ *ncolors,     /* palette size (<= 256) */
    sixel_allocator_t   /* in */  *allocator);  /* allocator object */
```

## Perl interface

This package includes a perl module `Image::Sixel`.

### Build and install Perl interface

```
$ cd perl
$ perl Build.PL
$ ./Build test
$ ./Build install
```

## Python interface

This package includes a Python module `libsixel`.

### Build and install Python interface

#### Install into the python prefixed with '/usr/local'

```
$ git clone https://github.com/saitoha/libsixel.git
$ cd libsixel
$ git checkout develop  # now available only develop branch
$ ./configure --enable-python --prefix=/usr/local
$ make install
```

#### Install into only current active python

```
$ git clone https://github.com/saitoha/libsixel.git
$ cd libsixel
$ git checkout develop  # now available only develop branch
$ ./configure --disable-python
$ make install  # install libsixel
$ cd python
$ python setup.py install  # install python module
```

or

```
$ easy_install libsixel-python
```

## PHP interface

This package includes a PHP module `sixel`.

### Build and install PHP interface

```
$ cd php/sixel
$ phpize
$ ./configure
$ make install
```

## Ruby interface

### Build and install Ruby interface

```
$ gem install libsixel-ruby
```

or

```
$ git submodule update --init
$ rake compile
$ rake build install
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

## Contributers and Reviewers

- [@arakiken](https://github.com/arakiken/)
- [@elfring](https://github.com/elfring/)
- [@fd00](https://github.com/fd00/)
- [@hattya](https://github.com/hattya/)
- [@isaki68k](https://github.com/isaki68k/)
- [@knok](https://github.com/knok/)
- [@mattn](https://github.com/mattn/)
- [@msmhrt](https://github.com/msmhrt/)
- [@obache](https://github.com/obache/)
- [@tsutsui](https://github.com/tsutsui/)
- [@ttdoda](https://github.com/ttdoda/)
- [@turenar](https://github.com/turenar/)
- [@uobikiemukot](https://github.com/uobikiemukot/)
- [@vrtsds](https://github.com/vrtsds/)
- [@waywardmonkeys](https://github.com/waywardmonkeys/)
- [@yoshikaw](https://github.com/yoshikaw/)
- [@turenar](https://github.com/turenar/)
- [@mame](https://github.com/mame/)
- [@hodefoting](https://github.com/hodefoting/)
- [@fCorleone](@https://github.com/fCorleone)
- [@fgeek](https://github.com/fgeek/)
- [@HongxuChen](https://github.com/HongxuChen/)
- [@YourButterfly](https://github.com/YourButterfly/)
- [@nluedtke](https://github.com/nluedtke/)
- [@cool-tomato](https://github.com/cool-tomato/)

## Contributing

1. Fork it ( https://github.com/saitoha/libsixel/fork/ )
2. Create your feature branch (git checkout -b my-new-feature)
3. Commit your changes (git commit -am 'Add some feature')
4. Push to the branch (git push origin my-new-feature)
5. Create a new Pull Request


## Acknowledgment

This software derives from the following implementations.

### sixel 2014-3-2

src/tosixel.c, src/fromsixel.c, and some part of converters/loader.c are
derived from kmiya's "*sixel*" original version (2014-3-2)

  Package: http://nanno.dip.jp/softlib/man/rlogin/sixel.tar.gz

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

This software includes `stb-image-v2.12` (stb_image.h),
public domain JPEG/PNG reader.

https://github.com/nothings/stb

> LICENSE
>
> This software is in the public domain. Where that dedication is not
> recognized, you are granted a perpetual, irrevocable license to copy,
> distribute, and modify this file as you see fit.


### stbiw-1.02
This software includes `stb-image-write-v1.02` (stb_image_write.h),
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

http://netpbm.sourceforge.net/

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


### ax_gcc_var_attribute / ax_gcc_func_attribute

These are useful m4 macros for detecting some GCC attributes / built-in functions.

http://www.gnu.org/software/autoconf-archive/ax_gcc_var_attribute.html
http://www.gnu.org/software/autoconf-archive/ax_gcc_func_attribute.html
http://www.gnu.org/software/autoconf-archive/ax_gcc_builtin.html

> Copyright (c) 2013 Gabriele Svelto <gabriele.svelto@gmail.com>
>
> Copying and distribution of this file, with or without modification, are
> permitted in any medium without royalty provided the copyright notice
> and this notice are preserved.  This file is offered as-is, without any
> warranty.


### graphics.c (from Xterm pl#310)

The helper function `hls2rgb` in `src/fromsixel.c` is imported from
`graphics.c` in [Xterm pl#310](http://invisible-island.net/xterm/),
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


#### PngSuite

Images under the directory images/pngsuite/ are imported from
[PngSuite](http://www.schaik.com/pngsuite/) created by Willem van Schaik.

> Permission to use, copy, modify and distribute these images for any
> purpose and without fee is hereby granted.
>
>
>(c) Willem van Schaik, 1996, 2011


## References

### ImageMagick

We are greatly inspired by the quality of ImageMagick and added some resampling filters to
`img2sixel` in reference to the line-up of filters of MagickCore's resize.c.

    http://www.imagemagick.org/api/MagickCore/resize_8c_source.html


## Similar software

- [netpbm](http://netpbm.sourceforge.net/)

  You can get SIXEL graphics using [ppmtosixel](http://netpbm.sourceforge.net/doc/ppmtosixel.html)
  or [pbmtoln03](http://netpbm.sourceforge.net/doc/ppmtosixel.html).


- [kmiya's sixel](http://nanno.dip.jp/softlib/man/rlogin/sixel.tar.gz)

  libgd based SIXEL converter


- [PySixel](https://pypi.python.org/pypi/PySixel)

  Python implementation of SIXEL converter


- [ImageMagick](http://www.imagemagick.org/)

  Now SIXEL coder is available in svn trunk and V6 branch.


- [monosixel in arakiken's tw](https://bitbucket.org/arakiken/tw/branch/sixel)

  A monochrome SIXEL converter


- [sixelv in sayaka-chan Vala version](https://github.com/isaki68k/sayaka/blob/master/vala/sixelv.vala)

  sayaka-chan(Vala version) also includes SIXEL converter.


- [rust-sixel](https://github.com/meh/rust-sixel)

  A SIXEL encoder written in rust.


- [forth-sixel](https://hub.darcs.net/pointfree/forth-sixel)

  A SIXEL encoder written in forth.


- [ff2sixel](https://github.com/labdsf/ff2sixel)

  An utility to convert farbfeld images to Sixels.


- [tv](https://github.com/hodefoting/tv)

  terminal/commandline image viewer


- [xpr(x11-apps)](ftp://ftp.x.org/pub/unsupported/programs/xpr/)

  xpr(1) can convert a xwd(X window dump) format image into a sixel
  image with '-device ln03' or '-device la100' option.
  But now it is not maintained. It looks broken.


## Other software supporting SIXEL

- [GNUPLOT](http://www.gnuplot.info/)

  Recent version of GNUPLOT supports new terminal driver "sixeltek(sixel)" / "sixelgd".

  ![GNUPLOT](https://raw.githubusercontent.com/saitoha/libsixel/data/data/gnuplot.png)


- [ghostscript](http://www.ghostscript.com/)

  You can emit SIXEL images with LN03 / LN50 / LA75 driver.

  example:

  ```
    $ gs -q -r100x -dBATCH -dNOPAUSE -sDEVICE=ln03 -sOutputFile=- tiger.eps
  ```

  ![GhostScript](https://raw.githubusercontent.com/saitoha/libsixel/data/data/gs.png)


- [ImageMagick](http://www.imagemagick.org/)

  Recent version of ImageMagick provides SIXEL coder. It's also available over commandline tools.

  ![ImageMagick](https://raw.githubusercontent.com/saitoha/libsixel/data/data/imagemagick.png)


- [lsix](https://github.com/hackerb9/lsix)

  Like "ls", but for images. Shows thumbnails in terminal using sixel graphics.
  ![lsix](https://raw.githubusercontent.com/saitoha/libsixel/data/data/lsix.jpg)


- [sixeldraw](https://github.com/aiju/sixeldraw)

  Sixel support for p9p devdraw

  cmapcube on xterm with DEVDRAW=sixeldraw
  [![sixeldraw2](https://raw.githubusercontent.com/saitoha/libsixel/data/data/sixeldraw2.png)](https://youtu.be/EOvSrt7Yi00)

  acme on xterm with DEVDRAW=sixeldraw SNARF=1
  [![sixeldraw1](https://raw.githubusercontent.com/saitoha/libsixel/data/data/sixeldraw1.png)](https://youtu.be/eGjSEjxiDjE)


- [ZX81 Emulator](http://rullf2.xs4all.nl/sg/zx81ce.html)

  A ZX81 emulator producing Sixel Image Files

  ![ZX81](https://raw.githubusercontent.com/saitoha/libsixel/data/data/zx81.png)


- [qrc](https://github.com/fumiyas/qrc)

  QR code generator for terminals (ASCII Art, Sixel)

  ![qrc](https://github.com/fumiyas/qrc/blob/master/qrc-demo.png)


- [go-sixel](https://github.com/mattn/go-sixel)

  SIXEL encoder/decoder and command line tools writtern in golang.

  ![go-sixel](https://raw.githubusercontent.com/saitoha/libsixel/data/data/go-sixel.png)


- [hiptext](https://github.com/jart/hiptext)

  SIXEL format is supported by -sixel2, -sixel16 or -sixel256 option.

  ![hiptext](https://camo.githubusercontent.com/fc973ffb20a7ff85969df03fd579da2845e62e68/68747470733a2f2f662e636c6f75642e6769746875622e636f6d2f6173736574732f313136323733392f323233393832362f39303361653765382d396335622d313165332d383462362d3539626261346661336430342e706e67)


- [sixelslide](https://github.com/syuu1228/sixelslide)

  Freestanding slideviewer using sixel graphics.
  Currently runs on QEMU(i386), without any filesystem or network.

  ![Animation](https://raw.githubusercontent.com/saitoha/libsixel/data/data/sixelslide.png)

  cf. http://www.slideshare.net/syuu1228/presentation-on-your-terminal


- [GraphicConverter](https://itunes.apple.com/jp/app/graphicconverter-9/id736099092?mt=12)

  It can import SIXEL images.

  ![GraphicConverter](https://raw.githubusercontent.com/saitoha/libsixel/data/data/graphicconverter.png)


- [SIXEL image viewer](http://rullf2.xs4all.nl/sg/sg.html)

  This web page can decode SIXEL images (written in javascript).

  ![SIXEL image viewer ](https://raw.githubusercontent.com/saitoha/libsixel/data/data/js-sixel.png)


- [mandel4](http://kildall.apana.org.au/~cjb/mandel5.c)

  A mandelbrot program for (colour) sixel-supporting terminals, written by Chris Baird

  ![mandel4](https://raw.githubusercontent.com/saitoha/libsixel/data/data/mandel.png)


- [SixelGraphics.jl(written in Julia)](https://github.com/olofsen/SixelGraphics.jl)

  A module for Julia implementing simple Sixel graphics.

  ![SixelGraphics.jl](https://raw.githubusercontent.com/saitoha/libsixel/data/data/julia.png)


- ![PGPLOT](http://www.astro.caltech.edu/~tjp/pgplot/)


- [SIXEL to PostScript converter](http://t.co/zTC7LhRbBc)


- [sixelplot](https://github.com/kktk-KO/sixelplot)

  thin-wrapper for pysixel and matplotlib


- [ame.sh](https://github.com/hamano/ame.sh)

- [matplotlib-sixel](https://github.com/koppa/matplotlib-sixel)

- [gr framework](http://gr-framework.org/)

- [o2sh/onefetch](https://github.com/o2sh/onefetch)

- [lesnitsky/sixel-decoder](https://github.com/lesnitsky/sixel-decoder)

- [unhappychoice/irasutoya-cli](https://github.com/unhappychoice/irasutoya-cli)

- [ushitora-anqou/tinysixel](https://github.com/ushitora-anqou/tinysixel)

- [adzierzanowski/timg](https://github.com/adzierzanowski/timg)

- [SAT1226/Minase](https://github.com/SAT1226/Minase)

- [danr/neptyne](https://github.com/danr/neptyne)

- [klamonte/jexer](https://github.com/klamonte/jexer)

- [ar90n/teimpy](https://github.com/ar90n/teimpy)

- [fastai](https://github.com/fastai/fastai)

- [coderobe/crixel](https://github.com/coderobe/crixel)

- [itchyny/mkrg](https://github.com/itchyny/mkrg)

- [tshort/SixelTerm.jl](https://github.com/tshort/SixelTerm.jl)

- [dsanson/termpdf](https://github.com/dsanson/termpdf)

- [otiai10/amesh](https://github.com/otiai10/amesh)

- [hpjansson/chafa](https://github.com/hpjansson/chafa)

- [m-j-w/TerminalGraphics.jl](https://github.com/m-j-w/TerminalGraphics.jl)

- [MIC-DKFZ/niicat](https://github.com/MIC-DKFZ/niicat)

- [libretro/RetroArch](https://github.com/libretro/RetroArch)

- [jerch/node-sixel](https://github.com/jerch/node-sixel)

- [nikiroo/fanfix](https://github.com/nikiroo/fanfix)

- [mattn/longcat](https://github.com/mattn/longcat)

- [ismail-yilmaz/upp-components/CtrlLib/Terminal/](https://github.com/ismail-yilmaz/upp-components/tree/master/CtrlLib/Terminal)

- [schrmh/pdfgrepSIXEL](schrmh/pdfgrepSIXEL)

- [ar90n/teimpy](https://github.com/ar90n/teimpy)

- [vifm/vifm](https://github.com/vifm/vifm)

- [ktye/iv](https://github.com/ktye/iv/commit/815e06ed776dde3deca0fdba35da5f0b431a69bf)

- [Delta/longdog](https://github.com/0Delta/longdog)
