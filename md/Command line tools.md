# Command line tools

## `img2sixel`

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

## `sixel2png`

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

