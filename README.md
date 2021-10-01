libsixel
========

![logo](https://avatars.githubusercontent.com/u/85618227)

[![Build Status](https://github.com/libsixel/libsixel/actions/workflows/build.yml/badge.svg)](https://github.com/libsixel/libsixel/actions/)

## This is a fork

This is a fork with various improvements and security patches. Hayaki Saito, the project's originator and long-time maintainer, disappeared in 1/2020. It's unknown what happened to him, he no longer even posts on Twitter. This is a continuation by the community. Fredrick R. Brennan (@ctrlcctrlv) is lead maintainer. For more information see [saitoha/libsixel issue №154](https://github.com/saitoha/libsixel/issues/154).

## What is this?

This package provides a C encoder/decoder implementation for DEC SIXEL graphics in the `libsixel.so` shared library, and two converter programs, `img2sixel` and `sixel2png`.

![img2sixel](https://raw.githubusercontent.com/saitoha/libsixel/data/data/libsixel-1.png)(https://youtu.be/0SasrQ7pnbA)

SIXEL is one of image formats for printer and terminal imaging introduced by
Digital Equipment Corp. (DEC).
Its data scheme is represented as a terminal-friendly escape sequence.
So if you want to view a SIXEL image file, all you have to do is "cat" it to your terminal.

On 80's real hardware terminals, it tooks unbearable long waiting times to display images.

[![vt330sixel](https://raw.githubusercontent.com/saitoha/libsixel/data/data/vt330sixel.png)](https://youtu.be/0SasrQ7pnbA)

But nowdays, with high-speed CPU and broadband network, we got the chance broaden the scope of SIXEL graphics.

## SIXEL animation

`img2sixel(1)` can decode GIF animations as well.

  ![Animation](https://raw.githubusercontent.com/saitoha/libsixel/data/data/sixel.gif)


## Projects using SIXEL graphics

### gnuplot

- [gnuplot](https://www.gnuplot.info/)

  Recent versions of gnuplot support the new terminal driver "sixeltek(sixel)" / "sixelgd".

  ![gnuplot](https://raw.githubusercontent.com/saitoha/libsixel/data/data/gnuplot.png)

### Ghostscript

- [ghostscript](https://www.ghostscript.com/)

  You can emit SIXEL images with LN03 / LN50 / LA75 driver.

  example:

  ```
    $ gs -q -r100x -dBATCH -dNOPAUSE -sDEVICE=ln03 -sOutputFile=- tiger.eps
  ```

  ![GhostScript](https://raw.githubusercontent.com/saitoha/libsixel/data/data/gs.png)

### ImageMagick

- [ImageMagick](https://www.imagemagick.org/)

  Recent version of ImageMagick provides SIXEL coder. It's also available over commandline tools.

  ![ImageMagick](https://raw.githubusercontent.com/saitoha/libsixel/data/data/imagemagick.png)

### `lsix`

- [lsix](https://github.com/hackerb9/lsix)

  Like "ls", but for images. Shows thumbnails in terminal using sixel graphics.
  ![lsix](https://raw.githubusercontent.com/saitoha/libsixel/data/data/lsix.jpg)

### Video streaming

Now Youtube video streaming is available over SIXEL protocol by [FFmpeg-SIXEL](https://github.com/saitoha/FFmpeg-SIXEL) project.

  [![FFmpeg-SIXEL](https://raw.githubusercontent.com/saitoha/libsixel/data/data/ffmpeg.png)](https://youtu.be/hqMh47lYHlc)

Above demo only uses 16 color registers.

### SDL integration: Gaming, Virtualization, ...etc.

[SDL1.2-SIXEL](https://github.com/saitoha/SDL1.2-SIXEL) project makes enable you to operate various GUI applications on the terminal.

You can play "`The Battle for Wesnoth`" over SIXEL protocol.

  [![SDL1.2-SIXEL WESNOTH](https://raw.githubusercontent.com/saitoha/libsixel/data/data/wesnoth.png)](https://youtu.be/aMUkN7TSct4)

You can run QEMU on SIXEL terminals.

  [![SDL1.2-SIXEL QEMU](https://raw.githubusercontent.com/saitoha/libsixel/data/data/qemu.png)](https://youtu.be/X6M5tgNjEuQ)

[SDL1.2-SIXEL](https://github.com/saitoha/SDL1.2-SIXEL) can collaborate with [XSDL-SIXEL](https://github.com/saitoha/xserver-xsdl-sixel).

  [![SDL1.2-SIXEL XSDL](https://raw.githubusercontent.com/saitoha/libsixel/data/data/xsdl.png)](https://youtu.be/UOTMGdUPYRo)

Furthermore some information of SIXEL-ready SDL applications are reported.

- [NetSurf](https://www.reddit.com/r/commandline/comments/4qyb90/netsurf_a_graphical_browser_on_xterm_using_sixel/)
  ([screenshot](https://imgur.com/a/Y6xH6))

- [Green PDF Viewer](https://www.reddit.com/r/commandline/comments/4oldf5/view_pdfs_in_terminal_requires_nixos_latest_git/)
  ([screenshot](https://m.reddit.com/r/commandline/comments/4oldf5/view_pdfs_in_terminal_requires_nixos_latest_git/))

- [DOOM](https://www.libsdl.org/projects/doom/)
  ([tweet](https://twitter.com/rattcv/status/775213402130046977))

- [firesdl](https://github.com/klange/firesdl)
  ([movie](https://www.youtube.com/watch?v=XubH2W39Xtc))


### `w3m` integration

`img2sixel(1)` can be integrated with [Debian's w3m](https://tracker.debian.org/pkg/w3m)(maintained by [Tatsuya Kinoshita](https://github.com/tats)) that includes patches for *-sixel* option derived from code by @arakiken (w3m `remoteimg` branch).

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

[Arakiken's GNU Screen fork(sixel branch)](https://github.com/csdvrx/sixel-gnuscreen)
works with SIXEL-supported applications including above products.
This project is now in progress.
GUI flavored SIXEL applications will integrated with existing terminal applications on it.

  ![w3m-sixel-screen](https://raw.githubusercontent.com/saitoha/libsixel/data/data/w3m-sixel-screen.png)

  ![sixel-screen](https://raw.githubusercontent.com/saitoha/libsixel/data/data/arakikens-screen.jpg)

  ![xsixel-screen](https://raw.githubusercontent.com/saitoha/libsixel/data/data/xsixel-on-screen.png)

See also on [youtube](https://youtu.be/QQAqe32VkFg).

### libsixel running on old hardware

Some NetBSD/OpenBSD users are doing challenging work, showing that SIXEL graphics work on old low-powered machines such as:

#### OMRON LUNA-II

  [NetBSD/luna68k](https://wiki.netbsd.org/ports/luna68k/) (here is OMRON LUNA-II):

  ![mikutterm-netbsd-luna68k](https://raw.githubusercontent.com/saitoha/libsixel/data/data/mikutterm-netbsd-luna68k.jpg)

#### HP9000/425e

  [NetBSD/hp300](https://wiki.netbsd.org/ports/hp300/) (here is HP9000/425e):

  ![mikutterm-netbsd-hp9000](https://raw.githubusercontent.com/saitoha/libsixel/data/data/mikutterm-netbsd-hp9000.jpg)

#### OMRON LUNA-88K2

  [OpenBSD/luna88k](https://www.openbsd.org/luna88k.html) (here is OMRON LUNA-88K2 MC88100@33MHz):

  ![mikutterm-netbsd-hp9000](https://raw.githubusercontent.com/saitoha/libsixel/data/data/tw-openbsd-luna88k.jpg)


### [sayaka-chan](https://github.com/isaki68k/sayaka/)

  [sayaka-chan](https://github.com/isaki68k/sayaka/)(PHP version) works with libsixel inline-image extension.

  SIXEL works with old powerless machines such as [NetBSD/x68k](https://wiki.netbsd.org/ports/x68k/) (here is SHARP X68030 with 060turbo):

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
  See <https://github.com/dylanaraps/neofetch/wiki/Image-Backends#sixel>

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

For even more, see [`Projects using SIXEL.md`](https://github.com/libsixel/libsixel/blob/master/md/Projects%20using%20SIXEL.md).

## Highlighted features

### Improved compression

Former sixel encoders(such as [ppmtosixel](https://netpbm.sourceforge.net/doc/ppmtosixel.html)) are mainly designed for dot-matrix printers.
They minimize the amount of printer-head movement distance.
But nowadays this method did not represent the best performance for displaying sixel data on terminal emulators.
SIXEL data for terminals were found in 80's Usenet, but the technology of how to create them seems to be lost.
[kmiya's sixel](https://nanno.dip.jp/softlib/man/rlogin/sixel.tar.gz) introduces the encoding method which is re-designed
for terminal emulators to optimize the overhead of transporting SIXEL with keeping compatibility with former SIXEL terminal.
Now libsixel and ImageMagick's sixel coder follow it.

@arakiken, known as the maintainer of mlterm, describes about the way to generate high quality SIXEL, which is adopted by libsixel
([https://mlterm.sourceforge.net/libsixel.pdf](https://mlterm.sourceforge.net/libsixel.pdf), in Japanese).

### High quality quantization

`img2sixel(1)` supports color image quantization. It works well even if few number of colors are allowed.

## Terminal requirements

If you want to view a SIXEL image, you have to get a terminal which support sixel graphics. Many terminals have support, such as `mlterm` and `iTerm2`. `xterm` is a commonly installed Linux terminal with support if ran as `xterm -xrm "XTerm*decTerminalID: vt340" -xrm "XTerm*numColorRegisters: 256"`. For a complete list of supported terminals, see [`Supported terminals.md`](https://github.com/libsixel/libsixel/blob/master/md/Supported%20terminals.md).

## Install

### Using package managers

You can install libsixel via the following package systems.

- [FreeBSD ports](https://portsmon.freebsd.org/portoverview.py?category=graphics&portname=libsixel)
- [DPorts](https://github.com/DragonFlyBSD/DPorts/tree/master/graphics/libsixel)
- [pkgsrc](https://cvsweb.netbsd.org/bsdweb.cgi/pkgsrc/graphics/libsixel/)
- [Homebrew](https://formulae.brew.sh/formula/libsixel)
- [yacp](https://github.com/fd00/yacp/tree/master/libsixel)
- [Debian](https://packages.debian.org/search?searchon=names&keywords=libsixel)
- [AUR](https://aur.archlinux.org/packages/libsixel-git/)
- [Portage](https://packages.gentoo.org/package/media-libs/libsixel)
- [Ubuntu](https://launchpad.net/ubuntu/+source/libsixel)
- [NixOS](https://github.com/NixOS/nixpkgs/blob/master/pkgs/development/libraries/libsixel/default.nix)
- [OpenBSD Ports](https://openports.se/graphics/libsixel)
- [Fedora Copr](https://copr.fedorainfracloud.org/coprs/saahriktu/libsixel/)
- [SlackBuilds](https://slackbuilds.org/repository/14.2/libraries/libsixel/)

### Build from source package

As of libsixel 2.0, Meson is used to build, not GNU Autotools.

```
$ meson setup build
$ meson install -C build
```

#### Build with optional packages

You can use the following options at build time to influence the build. During the `meson build` step, pass e.g. `-Dlibcurl=enabled` to enable cURL.

|Option name|Former GNU Autotools equivalent|Description|Default?|
|--- |--- |--- |--- |
|img2sixel|`--enable-img2sixel`|Build binary `img2sixel`|Yes|
|sixel2png|`--enable-sixel2png`|Build binary `sixel2png`|Yes|
|gdk-pixbuf2|`--with-gdk-pixbuf2`|Whether to build in gdk-pixbuf2 support|No|
|gd|`--with-gd`|Whether to build in gd support (adds more image formats)|Auto|
|libcurl|`--with-libcurl`|build with cURL (allows input filenames to binaries/API to be URLs)|No|
|jpeg|`--with-jpeg`|Whether to build with libjpeg support|Auto|
|png|`--with-png`|Whether to build with libpng support|Auto|
|gcov|`--enable-gcov`|Build gcov coverage tests|No|
|tests|`--enable-tests`|Build tests (requires `bash`)|No|
|python3|`--enable-python3`|Build Python library integration|No|
|pkg\_config\_path|`--with-pkgconfigdir`|`pkg-config` search directory|Set by Meson|

As well, several directories can be configured, most importantly `prefix`. Non-standard directories you can change are `bashcompletiondir` and `zshcompletiondir`.

Note: Before libsixel 2.0, Python was installed by default. This was disabled because it requires root on most systems for the Python module to be discoverable. Pass `-Dpython3=enabled` to install it.

## Usage of command line tools

See [`Command line tools.md`](https://github.com/libsixel/libsixel/blob/master/md/Command%20line%20tools.md).

## Examples

The following three example projects are distributed in the `examples/` directory:

### OpenGL

[OpenGL example](https://github.com/saitoha/libsixel/blob/master/examples/opengl/)
suggests how to port your OpenGL application to a SIXEL-supporting terminal.

  ![opengl example](https://raw.githubusercontent.com/saitoha/libsixel/data/data/example_opengl.gif)

### Drawing

[Drawing example](https://github.com/saitoha/libsixel/blob/master/examples/drawing/)
suggests how to implement drawing with the mouse in SIXEL-supporting terminals.

  [![drawing](https://raw.githubusercontent.com/saitoha/libsixel/data/data/drawing.png)](https://youtu.be/2-2FnoZp4Z0)


### Python

[Python example](https://github.com/saitoha/libsixel/blob/master/examples/python/)
suggests how to convert PIL images into SIXEL graphics using libsixel's Python interface.

## APIs

APIs are provided for C, Python, Perl, PHP, and Ruby. For documentation of the C API, see the file [`md/C API.md`](https://github.com/libsixel/libsixel/blob/master/md/C%20API.md), or its header, [`sixel.h`](https://raw.githubusercontent.com/libsixel/libsixel/master/include/sixel.h.in). For documentations of the APIs for scripting languages, see the README for them in their respective directories.

## Support

This software is provided "as is" without express or implied warranty.
The main support channel for this software is its GitHub issue tracker:

  [https://github.com/libsixel/libsixel/issues](https://github.com/libsixel/libsixel/issues)

Please post an issue if you have any problems, questions or suggestions.

## License

libsixel is MIT-licensed. See the [`LICENSE`](https://github.com/libsixel/libsixel/blob/master/LICENSE) file. For licenses of vendorized code, see the files in the [`licenses`](https://github.com/libsixel/libsixel/blob/master/licenses/) directory.

For a list of authors, see the [`AUTHORS`](https://github.com/libsixel/libsixel/blob/master/AUTHORS) file.

## Contributing

1. Fork the project (<https://github.com/libsixel/libsixel/fork/>)
2. Pull (`git clone <your URL>`)
3. Create your feature branch (`git checkout -b my-new-feature`)
4. Make your changes and add your changed files (`git add ...`)
5. Commit your changes (`git commit -am 'Add some feature'`)
6. Push to the branch (`git push origin my-new-feature`)
7. Create a new Pull Request

## Acknowledgments

See [`Acknowledgements.md`](https://github.com/libsixel/libsixel/blob/master/md/Acknowledgements.md).

## Other implementations and similar software

See [`Other implementations.md`](https://github.com/libsixel/libsixel/blob/master/md/Other%20implementations.md).
