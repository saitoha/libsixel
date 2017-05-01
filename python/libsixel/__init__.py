#!/usr/bin/env python
#
# Copyright (c) 2014-2016 Hayaki Saito
#
# Permission is hereby granted, free of charge, to any person obtaining a copy of
# this software and associated documentation files (the "Software"), to deal in
# the Software without restriction, including without limitation the rights to
# use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of
# the Software, and to permit persons to whom the Software is furnished to do so,
# subject to the following conditions:
#
# The above copyright notice and this permission notice shall be included in all
# copies or substantial portions of the Software.
#
# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
# IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS
# FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR
# COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER
# IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
# CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
#

from ctypes import cdll, c_void_p, c_int, c_byte, c_char_p, POINTER, byref, CFUNCTYPE, string_at
from ctypes.util import find_library

SIXEL_OK              = 0x0000
SIXEL_FALSE           = 0x1000

SIXEL_RUNTIME_ERROR   = (SIXEL_FALSE         | 0x0100)  # runtime error
SIXEL_LOGIC_ERROR     = (SIXEL_FALSE         | 0x0200)  # logic error
SIXEL_FEATURE_ERROR   = (SIXEL_FALSE         | 0x0300)  # feature not enabled
SIXEL_LIBC_ERROR      = (SIXEL_FALSE         | 0x0400)  # errors caused by curl
SIXEL_CURL_ERROR      = (SIXEL_FALSE         | 0x0500)  # errors occures in libc functions
SIXEL_JPEG_ERROR      = (SIXEL_FALSE         | 0x0600)  # errors occures in libjpeg functions
SIXEL_PNG_ERROR       = (SIXEL_FALSE         | 0x0700)  # errors occures in libpng functions
SIXEL_GDK_ERROR       = (SIXEL_FALSE         | 0x0800)  # errors occures in gdk functions
SIXEL_GD_ERROR        = (SIXEL_FALSE         | 0x0900)  # errors occures in gd functions
SIXEL_STBI_ERROR      = (SIXEL_FALSE         | 0x0a00)  # errors occures in stb_image functions
SIXEL_STBIW_ERROR     = (SIXEL_FALSE         | 0x0b00)  # errors occures in stb_image_write functions

SIXEL_INTERRUPTED     = (SIXEL_OK            | 0x0001)  # interrupted by a signal

SIXEL_BAD_ALLOCATION  = (SIXEL_RUNTIME_ERROR | 0x0001)  # malloc() failed
SIXEL_BAD_ARGUMENT    = (SIXEL_RUNTIME_ERROR | 0x0002)  # bad argument detected
SIXEL_BAD_INPUT       = (SIXEL_RUNTIME_ERROR | 0x0003)  # bad input detected

SIXEL_NOT_IMPLEMENTED = (SIXEL_FEATURE_ERROR | 0x0001)  # feature not implemented

def SIXEL_SUCCEEDED(status):
    return (((status) & 0x1000) == 0)

def SIXEL_FAILED(status):
    return (((status) & 0x1000) != 0)

# method for finding the largest dimension for splitting,
# and sorting by that component
SIXEL_LARGE_AUTO = 0x0   # choose automatically the method for finding the largest dimension
SIXEL_LARGE_NORM = 0x1   # simply comparing the range in RGB space
SIXEL_LARGE_LUM  = 0x2   # transforming into luminosities before the comparison

# method for choosing a color from the box
SIXEL_REP_AUTO           = 0x0  # choose automatically the method for selecting representative color from each box
SIXEL_REP_CENTER_BOX     = 0x1  # choose the center of the box
SIXEL_REP_AVERAGE_COLORS = 0x2  # choose the average all the color in the box (specified in Heckbert's paper)
SIXEL_REP_AVERAGE_PIXELS = 0x3  # choose the average all the pixels in the box

# method for diffusing
SIXEL_DIFFUSE_AUTO      = 0x0  # choose diffusion type automatically
SIXEL_DIFFUSE_NONE      = 0x1  # don't diffuse
SIXEL_DIFFUSE_ATKINSON  = 0x2  # diffuse with Bill Atkinson's method
SIXEL_DIFFUSE_FS        = 0x3  # diffuse with Floyd-Steinberg method
SIXEL_DIFFUSE_JAJUNI    = 0x4  # diffuse with Jarvis, Judice & Ninke method
SIXEL_DIFFUSE_STUCKI    = 0x5  # diffuse with Stucki's method
SIXEL_DIFFUSE_BURKES    = 0x6  # diffuse with Burkes' method
SIXEL_DIFFUSE_A_DITHER  = 0x7  # diffuse with pippin's a_dither method
SIXEL_DIFFUSE_X_DITHER  = 0x8  # diffuse with pippin's x_dither method

# quality modes
SIXEL_QUALITY_AUTO      = 0x0  # choose quality mode automatically
SIXEL_QUALITY_HIGH      = 0x1  # high quality palette construction
SIXEL_QUALITY_LOW       = 0x2  # low quality palette construction
SIXEL_QUALITY_FULL      = 0x3  # full quality palette construction
SIXEL_QUALITY_HIGHCOLOR = 0x4  # high color

# built-in dither
SIXEL_BUILTIN_MONO_DARK   = 0x0  # monochrome terminal with dark background
SIXEL_BUILTIN_MONO_LIGHT  = 0x1  # monochrome terminal with dark background
SIXEL_BUILTIN_XTERM16     = 0x2  # xterm 16color
SIXEL_BUILTIN_XTERM256    = 0x3  # xterm 256color
SIXEL_BUILTIN_VT340_MONO  = 0x4  # vt340 monochrome
SIXEL_BUILTIN_VT340_COLOR = 0x5  # vt340 color
SIXEL_BUILTIN_G1          = 0x6  # 1bit grayscale
SIXEL_BUILTIN_G2          = 0x7  # 2bit grayscale
SIXEL_BUILTIN_G4          = 0x8  # 4bit grayscale
SIXEL_BUILTIN_G8          = 0x9  # 8bit grayscale

# offset value of pixelFormat
SIXEL_FORMATTYPE_COLOR     = (0)
SIXEL_FORMATTYPE_GRAYSCALE = (1 << 6)
SIXEL_FORMATTYPE_PALETTE   = (1 << 7)

# pixelformat type of input image
#   NOTE: for compatibility, the value of PIXELFORAMT_COLOR_RGB888 must be 3
SIXEL_PIXELFORMAT_RGB555   = (SIXEL_FORMATTYPE_COLOR     | 0x01) # 15bpp
SIXEL_PIXELFORMAT_RGB565   = (SIXEL_FORMATTYPE_COLOR     | 0x02) # 16bpp
SIXEL_PIXELFORMAT_RGB888   = (SIXEL_FORMATTYPE_COLOR     | 0x03) # 24bpp
SIXEL_PIXELFORMAT_BGR555   = (SIXEL_FORMATTYPE_COLOR     | 0x04) # 15bpp
SIXEL_PIXELFORMAT_BGR565   = (SIXEL_FORMATTYPE_COLOR     | 0x05) # 16bpp
SIXEL_PIXELFORMAT_BGR888   = (SIXEL_FORMATTYPE_COLOR     | 0x06) # 24bpp
SIXEL_PIXELFORMAT_ARGB8888 = (SIXEL_FORMATTYPE_COLOR     | 0x10) # 32bpp
SIXEL_PIXELFORMAT_RGBA8888 = (SIXEL_FORMATTYPE_COLOR     | 0x11) # 32bpp
SIXEL_PIXELFORMAT_G1       = (SIXEL_FORMATTYPE_GRAYSCALE | 0x00) # 1bpp grayscale
SIXEL_PIXELFORMAT_G2       = (SIXEL_FORMATTYPE_GRAYSCALE | 0x01) # 2bpp grayscale
SIXEL_PIXELFORMAT_G4       = (SIXEL_FORMATTYPE_GRAYSCALE | 0x02) # 4bpp grayscale
SIXEL_PIXELFORMAT_G8       = (SIXEL_FORMATTYPE_GRAYSCALE | 0x03) # 8bpp grayscale
SIXEL_PIXELFORMAT_AG88     = (SIXEL_FORMATTYPE_GRAYSCALE | 0x13) # 16bpp gray+alpha
SIXEL_PIXELFORMAT_GA88     = (SIXEL_FORMATTYPE_GRAYSCALE | 0x23) # 16bpp gray+alpha
SIXEL_PIXELFORMAT_PAL1     = (SIXEL_FORMATTYPE_PALETTE   | 0x00) # 1bpp palette
SIXEL_PIXELFORMAT_PAL2     = (SIXEL_FORMATTYPE_PALETTE   | 0x01) # 2bpp palette
SIXEL_PIXELFORMAT_PAL4     = (SIXEL_FORMATTYPE_PALETTE   | 0x02) # 4bpp palette
SIXEL_PIXELFORMAT_PAL8     = (SIXEL_FORMATTYPE_PALETTE   | 0x03) # 8bpp palette

# palette type
SIXEL_PALETTETYPE_AUTO     = 0   # choose palette type automatically
SIXEL_PALETTETYPE_HLS      = 1   # HLS colorspace
SIXEL_PALETTETYPE_RGB      = 2   # RGB colorspace

# policies of SIXEL encoding
SIXEL_ENCODEPOLICY_AUTO    = 0   # choose encoding policy automatically
SIXEL_ENCODEPOLICY_FAST    = 1   # encode as fast as possible
SIXEL_ENCODEPOLICY_SIZE    = 2   # encode to as small sixel sequence as possible

# method for re-sampling
SIXEL_RES_NEAREST          = 0   # Use nearest neighbor method
SIXEL_RES_GAUSSIAN         = 1   # Use guaussian filter
SIXEL_RES_HANNING          = 2   # Use hanning filter
SIXEL_RES_HAMMING          = 3   # Use hamming filter
SIXEL_RES_BILINEAR         = 4   # Use bilinear filter
SIXEL_RES_WELSH            = 5   # Use welsh filter
SIXEL_RES_BICUBIC          = 6   # Use bicubic filter
SIXEL_RES_LANCZOS2         = 7   # Use lanczos-2 filter
SIXEL_RES_LANCZOS3         = 8   # Use lanczos-3 filter
SIXEL_RES_LANCZOS4         = 9   # Use lanczos-4 filter

# image format
SIXEL_FORMAT_GIF           = 0x0 # read only
SIXEL_FORMAT_PNG           = 0x1 # read/write
SIXEL_FORMAT_BMP           = 0x2 # read only
SIXEL_FORMAT_JPG           = 0x3 # read only
SIXEL_FORMAT_TGA           = 0x4 # read only
SIXEL_FORMAT_WBMP          = 0x5 # read only with --with-gd configure option
SIXEL_FORMAT_TIFF          = 0x6 # read only
SIXEL_FORMAT_SIXEL         = 0x7 # read only
SIXEL_FORMAT_PNM           = 0x8 # read only
SIXEL_FORMAT_GD2           = 0x9 # read only with --with-gd configure option
SIXEL_FORMAT_PSD           = 0xa # read only
SIXEL_FORMAT_HDR           = 0xb # read only

# loop mode
SIXEL_LOOP_AUTO            = 0   # honer the setting of GIF header
SIXEL_LOOP_FORCE           = 1   # always enable loop
SIXEL_LOOP_DISABLE         = 2   # always disable loop

# setopt flags
SIXEL_OPTFLAG_INPUT            = 'i'  # -i, --input: specify input file name.
SIXEL_OPTFLAG_OUTPUT           = 'o'  # -o, --output: specify output file name.
SIXEL_OPTFLAG_OUTFILE          = 'o'  # -o, --outfile: specify output file name.
SIXEL_OPTFLAG_7BIT_MODE        = '7'  # -7, --7bit-mode: for 7bit terminals or printers (default)
SIXEL_OPTFLAG_8BIT_MODE        = '8'  # -8, --8bit-mode: for 8bit terminals or printers
SIXEL_OPTFLAG_COLORS           = 'p'  # -p COLORS, --colors=COLORS: specify number of colors
SIXEL_OPTFLAG_MAPFILE          = 'm'  # -m FILE, --mapfile=FILE: specify set of colors
SIXEL_OPTFLAG_MONOCHROME       = 'e'  # -e, --monochrome: output monochrome sixel image
SIXEL_OPTFLAG_INSECURE         = 'k'  # -k, --insecure: allow to connect to SSL sites without certs
SIXEL_OPTFLAG_INVERT           = 'i'  # -i, --invert: assume the terminal background color
SIXEL_OPTFLAG_HIGH_COLOR       = 'I'  # -I, --high-color: output 15bpp sixel image
SIXEL_OPTFLAG_USE_MACRO        = 'u'  # -u, --use-macro: use DECDMAC and DEVINVM sequences
SIXEL_OPTFLAG_MACRO_NUMBER     = 'n'  # -n MACRONO, --macro-number=MACRONO:
                                      #        specify macro register number
SIXEL_OPTFLAG_COMPLEXION_SCORE = 'C'  # -C COMPLEXIONSCORE, --complexion-score=COMPLEXIONSCORE:
                                      #        specify an number argument for the score of
                                      #        complexion correction.
SIXEL_OPTFLAG_IGNORE_DELAY     = 'g'  # -g, --ignore-delay: render GIF animation without delay
SIXEL_OPTFLAG_STATIC           = 'S'  # -S, --static: render animated GIF as a static image
SIXEL_OPTFLAG_DIFFUSION        = 'd'  # -d DIFFUSIONTYPE, --diffusion=DIFFUSIONTYPE:
                                      #          choose diffusion method which used with -p option.
                                      #          DIFFUSIONTYPE is one of them:
                                      #            auto     -> choose diffusion type
                                      #                        automatically (default)
                                      #            none     -> do not diffuse
                                      #            fs       -> Floyd-Steinberg method
                                      #            atkinson -> Bill Atkinson's method
                                      #            jajuni   -> Jarvis, Judice & Ninke
                                      #            stucki   -> Stucki's method
                                      #            burkes   -> Burkes' method

SIXEL_OPTFLAG_FIND_LARGEST     = 'f'  # -f FINDTYPE, --find-largest=FINDTYPE:
                                      #         choose method for finding the largest
                                      #         dimension of median cut boxes for
                                      #         splitting, make sense only when -p
                                      #         option (color reduction) is
                                      #         specified
                                      #         FINDTYPE is one of them:
                                      #           auto -> choose finding method
                                      #                   automatically (default)
                                      #           norm -> simply comparing the
                                      #                   range in RGB space
                                      #           lum  -> transforming into
                                      #                   luminosities before the
                                      #                   comparison

SIXEL_OPTFLAG_SELECT_COLOR     = 's'  # -s SELECTTYPE, --select-color=SELECTTYPE
                                      #        choose the method for selecting
                                      #        representative color from each
                                      #        median-cut box, make sense only
                                      #        when -p option (color reduction) is
                                      #        specified
                                      #        SELECTTYPE is one of them:
                                      #          auto      -> choose selecting
                                      #                       method automatically
                                      #                       (default)
                                      #          center    -> choose the center of
                                      #                       the box
                                      #          average    -> calculate the color
                                      #                       average into the box
                                      #          histogram -> similar with average
                                      #                       but considers color
                                      #                       histogram

SIXEL_OPTFLAG_CROP             = 'c'  # -c REGION, --crop=REGION:
                                      #        crop source image to fit the
                                      #        specified geometry. REGION should
                                      #        be formatted as '%dx%d+%d+%d'

SIXEL_OPTFLAG_WIDTH            = 'w'  # -w WIDTH, --width=WIDTH:
                                      #        resize image to specified width
                                      #        WIDTH is represented by the
                                      #        following syntax
                                      #          auto       -> preserving aspect
                                      #                        ratio (default)
                                      #          <number>%  -> scale width with
                                      #                        given percentage
                                      #          <number>   -> scale width with
                                      #                        pixel counts
                                      #          <number>px -> scale width with
                                      #                        pixel counts

SIXEL_OPTFLAG_HEIGHT           = 'h'  # -h HEIGHT, --height=HEIGHT:
                                      #         resize image to specified height
                                      #         HEIGHT is represented by the
                                      #         following syntax
                                      #           auto       -> preserving aspect
                                      #                         ratio (default)
                                      #           <number>%  -> scale height with
                                      #                         given percentage
                                      #           <number>   -> scale height with
                                      #                         pixel counts
                                      #           <number>px -> scale height with
                                      #                         pixel counts

SIXEL_OPTFLAG_RESAMPLING       = 'r'  # -r RESAMPLINGTYPE, --resampling=RESAMPLINGTYPE:
                                      #        choose resampling filter used
                                      #        with -w or -h option (scaling)
                                      #        RESAMPLINGTYPE is one of them:
                                      #          nearest  -> Nearest-Neighbor
                                      #                      method
                                      #          gaussian -> Gaussian filter
                                      #          hanning  -> Hanning filter
                                      #          hamming  -> Hamming filter
                                      #          bilinear -> Bilinear filter
                                      #                      (default)
                                      #          welsh    -> Welsh filter
                                      #          bicubic  -> Bicubic filter
                                      #          lanczos2 -> Lanczos-2 filter
                                      #          lanczos3 -> Lanczos-3 filter
                                      #          lanczos4 -> Lanczos-4 filter

SIXEL_OPTFLAG_QUALITY          = 'q'  # -q QUALITYMODE, --quality=QUALITYMODE:
                                      #        select quality of color
                                      #        quanlization.
                                      #          auto -> decide quality mode
                                      #                  automatically (default)
                                      #          low  -> low quality and high
                                      #                  speed mode
                                      #          high -> high quality and low
                                      #                  speed mode
                                      #          full -> full quality and careful
                                      #                  speed mode

SIXEL_OPTFLAG_LOOPMODE         = 'l'  # -l LOOPMODE, --loop-control=LOOPMODE:
                                      #        select loop control mode for GIF
                                      #        animation.
                                      #          auto    -> honor the setting of
                                      #                     GIF header (default)
                                      #          force   -> always enable loop
                                      #          disable -> always disable loop

SIXEL_OPTFLAG_PALETTE_TYPE     = 't'  # -t PALETTETYPE, --palette-type=PALETTETYPE:
                                      #        select palette color space type
                                      #          auto -> choose palette type
                                      #                  automatically (default)
                                      #          hls  -> use HLS color space
                                      #          rgb  -> use RGB color space

SIXEL_OPTFLAG_BUILTIN_PALETTE  = 'b'  # -b BUILTINPALETTE, --builtin-palette=BUILTINPALETTE:
                                      #        select built-in palette type
                                      #          xterm16    -> X default 16 color map
                                      #          xterm256   -> X default 256 color map
                                      #          vt340mono  -> VT340 monochrome map
                                      #          vt340color -> VT340 color map
                                      #          gray1      -> 1bit grayscale map
                                      #          gray2      -> 2bit grayscale map
                                      #          gray4      -> 4bit grayscale map
                                      #          gray8      -> 8bit grayscale map

SIXEL_OPTFLAG_ENCODE_POLICY    = 'E'  # -E ENCODEPOLICY, --encode-policy=ENCODEPOLICY:
                                      #        select encoding policy
                                      #          auto -> choose encoding policy
                                      #                  automatically (default)
                                      #          fast -> encode as fast as possible
                                      #          size -> encode to as small sixel
                                      #                  sequence as possible

SIXEL_OPTFLAG_BGCOLOR          = 'B'  # -B BGCOLOR, --bgcolor=BGCOLOR:
                                      #        specify background color
                                      #        BGCOLOR is represented by the
                                      #        following syntax
                                      #          #rgb
                                      #          #rrggbb
                                      #          #rrrgggbbb
                                      #          #rrrrggggbbbb
                                      #          rgb:r/g/b
                                      #          rgb:rr/gg/bb
                                      #          rgb:rrr/ggg/bbb
                                      #          rgb:rrrr/gggg/bbbb

SIXEL_OPTFLAG_PENETRATE        = 'P'  # -P, --penetrate:
                                      #        penetrate GNU Screen using DCS
                                      #        pass-through sequence
SIXEL_OPTFLAG_PIPE_MODE        = 'D'  # -D, --pipe-mode:
                                      #         read source images from stdin continuously
SIXEL_OPTFLAG_VERBOSE          = 'v'  # -v, --verbose: show debugging info
SIXEL_OPTFLAG_VERSION          = 'V'  # -V, --version: show version and license info
SIXEL_OPTFLAG_HELP             = 'H'  # -H, --help: show this help

if not find_library('sixel'):
    raise ImportError("libsixel not found.")

# load shared library
_sixel = cdll.LoadLibrary(find_library('sixel'))

# convert error status code int formatted string
def sixel_helper_format_error(status):
    _sixel.sixel_helper_format_error.restype = c_char_p;
    _sixel.sixel_helper_format_error.argtypes = [c_int];
    return _sixel.sixel_helper_format_error(status)


# compute pixel depth from pixelformat
def sixel_helper_compute_depth(pixelformat):
    _sixel.sixel_helper_compute_depth.restype = c_int
    _sixel.sixel_encoder_encode.argtypes = [c_int]
    return _sixel.sixel_helper_compute_depth(pixelformat)


# create new output context object
def sixel_output_new(fn_write, priv=None, allocator=c_void_p(None)):
    def _fn_write_local(data, size, priv_from_c):
        fn_write(string_at(data, size), priv)
        return size
    sixel_write_function = CFUNCTYPE(c_int, c_char_p, c_int, c_void_p)
    _sixel.sixel_output_new.restype = c_int
    _sixel.sixel_output_new.argtypes = [POINTER(c_void_p), sixel_write_function, c_void_p, c_void_p]
    output = c_void_p(None)
    _fn_write = sixel_write_function(_fn_write_local)
    _fn_write.restype = c_int
    _fn_write.argtypes = [sixel_write_function, c_void_p, c_void_p]
    status = _sixel.sixel_output_new(byref(output), _fn_write, c_void_p(None), allocator)
    if SIXEL_FAILED(status):
        message = sixel_helper_format_error(status)
        raise RuntimeError(message)
    output.__fn_write = _fn_write
    return output


# increase reference count of output object (thread-unsafe)
def sixel_output_ref(output):
    _sixel.sixel_output_ref.restype = None
    _sixel.sixel_output_ref.argtypes = [c_void_p]
    _sixel.sixel_output_ref(output)


# decrease reference count of output object (thread-unsafe)
def sixel_output_unref(output):
    _sixel.sixel_output_unref.restype = None
    _sixel.sixel_output_unref.argtypes = [c_void_p]
    _sixel.sixel_output_unref(output)
    output.__fn_write = None


# get 8bit output mode which indicates whether it uses C1 control characters
def sixel_output_get_8bit_availability(output):
    _sixel.sixel_output_get_8bit_availability.restype = None
    _sixel.sixel_output_get_8bit_availability.argtypes = [c_void_p]
    _sixel.sixel_output_get_8bit_availability(output)


# set 8bit output mode state
def sixel_output_set_8bit_availability(output):
    _sixel.sixel_output_set_8bit_availability.restype = None
    _sixel.sixel_output_set_8bit_availability.argtypes = [c_void_p, c_int]
    _sixel.sixel_output_set_8bit_availability(output)


# set whether limit arguments of DECGRI('!') to 255
def sixel_output_set_gri_arg_limit(output):
    _sixel.sixel_output_set_gri_arg_limit.restype = None
    _sixel.sixel_output_set_gri_arg_limit.argtypes = [c_void_p, c_int]
    _sixel.sixel_output_set_gri_arg_limit(output)


# set GNU Screen penetration feature enable or disable
def sixel_output_set_penetrate_multiplexer(output):
    _sixel.sixel_output_set_penetrate_multiplexer.restype = None
    _sixel.sixel_output_set_penetrate_multiplexer.argtypes = [c_void_p, c_int]
    _sixel.sixel_output_set_penetrate_multiplexer(output)


# set whether we skip DCS envelope
def sixel_output_set_skip_dcs_envelope(output):
    _sixel.sixel_output_set_skip_dcs_envelope.restype = None
    _sixel.sixel_output_set_skip_dcs_envelope.argtypes = [c_void_p, c_int]
    _sixel.sixel_output_set_skip_dcs_envelope(output)


# set palette type: RGB or HLS
def sixel_output_set_palette_type(output):
    _sixel.sixel_output_set_palette_type.restype = None
    _sixel.sixel_output_set_palette_type.argtypes = [c_void_p, c_int]
    _sixel.sixel_output_set_palette_type(output)


# set encodeing policy: auto, fast or size
def sixel_output_set_encode_policy(output):
    _sixel.sixel_output_set_encode_policy.restype = None
    _sixel.sixel_output_set_encode_policy.argtypes = [c_void_p, c_int]
    _sixel.sixel_output_set_encode_policy(output)


# create dither context object
def sixel_dither_new(ncolors, allocator=None):
    _sixel.sixel_dither_new.restype = c_int
    _sixel.sixel_dither_new.argtypes = [POINTER(c_void_p), c_int, c_void_p]
    dither = c_void_p(None)
    status = _sixel.sixel_dither_new(byref(dither), ncolors, allocator)
    if SIXEL_FAILED(status):
        message = sixel_helper_format_error(status)
        raise RuntimeError(message)
    return dither


# get built-in dither context object
def sixel_dither_get(builtin_dither):
    _sixel.sixel_dither_get.restype = c_void_p
    _sixel.sixel_dither_get.argtypes = [c_int]
    return _sixel.sixel_dither_get(builtin_dither)


# destroy dither context object
def sixel_dither_destroy(dither):
    _sixel.sixel_dither_destroy.restype = None
    _sixel.sixel_dither_destroy.argtypes = [c_void_p]
    return _sixel.sixel_dither_destroy(dither)


# increase reference count of dither context object (thread-unsafe)
def sixel_dither_ref(dither):
    _sixel.sixel_dither_ref.restype = None
    _sixel.sixel_dither_ref.argtypes = [c_void_p]
    return _sixel.sixel_dither_ref(dither)


# decrease reference count of dither context object (thread-unsafe)
def sixel_dither_unref(dither):
    _sixel.sixel_dither_unref.restype = None
    _sixel.sixel_dither_unref.argtypes = [c_void_p]
    return _sixel.sixel_dither_unref(dither)


# initialize internal palette from specified pixel buffer
def sixel_dither_initialize(dither, data, width, height, pixelformat,
                            method_for_largest=SIXEL_LARGE_AUTO,
                            method_for_rep=SIXEL_REP_AUTO,
                            quality_mode=SIXEL_QUALITY_AUTO):
    _sixel.sixel_dither_initialize.restype = c_int
    _sixel.sixel_dither_initialize.argtypes = [c_void_p, c_char_p, c_int, c_int, c_int,
                                              c_int, c_int, c_int]
    status = _sixel.sixel_dither_initialize(dither, data, width, height, pixelformat,
                                            method_for_largest,
                                            method_for_rep,
                                            quality_mode)
    if SIXEL_FAILED(status):
        message = sixel_helper_format_error(status)
        raise RuntimeError(message)


# set diffusion type, choose from enum methodForDiffuse
def sixel_dither_set_diffusion_type(dither, method_for_diffuse):
    _sixel.sixel_dither_set_diffusion_type.restype = None
    _sixel.sixel_dither_set_diffusion_type.argtypes = [c_void_p, c_int]
    _sixel.sixel_dither_set_diffusion_type(dither, method_for_diffuse)


# get number of palette colors
def sixel_dither_get_num_of_palette_colors(dither):
    _sixel.sixel_dither_get_num_of_palette_colors.restype = c_int
    _sixel.sixel_dither_get_num_of_palette_colors.argtypes = [c_void_p]
    return _sixel.sixel_dither_get_num_of_palette_colors(dither)


# get number of histogram colors */
def sixel_dither_get_num_of_histogram_colors(dither):
    _sixel.sixel_dither_get_num_of_histogram_colors.restype = c_int
    _sixel.sixel_dither_get_num_of_histogram_colors.argtypes = [c_void_p]
    return _sixel.sixel_dither_get_num_of_histogram_colors(dither)


def sixel_dither_get_palette(dither):
    _sixel.sixel_dither_get_palette.restype = c_char_p
    _sixel.sixel_dither_get_palette.argtypes = [c_void_p]
    cpalette = _sixel.sixel_dither_get_palette(dither)
    return [ord(c) for c in cpalette]


def sixel_dither_set_palette(dither, palette):
    _sixel.sixel_dither_set_palette.restype = None
    _sixel.sixel_dither_set_palette.argtypes = [c_void_p, c_char_p]
    cpalette = ''.join(map(chr, palette))
    _sixel.sixel_dither_set_palette(dither, cpalette)


def sixel_dither_set_complexion_score(dither, score):
    _sixel.sixel_dither_set_complexion_score.restype = None
    _sixel.sixel_dither_set_complexion_score.argtypes = [c_void_p, c_int]
    _sixel.sixel_dither_set_complexion_score(dither, score)


def sixel_dither_set_body_only(dither, bodyonly):
    _sixel.sixel_dither_set_body_only.restype = None
    _sixel.sixel_dither_set_body_only.argtypes = [c_void_p, c_int]
    _sixel.sixel_dither_set_body_only(dither, bodyonly)


def sixel_dither_set_optimize_palette(dither, do_opt):
    _sixel.sixel_dither_set_optimize_palette.restype = None
    _sixel.sixel_dither_set_optimize_palette.argtypes = [c_void_p, c_int]
    _sixel.sixel_dither_set_optimize_palette(dither, do_opt)


def sixel_dither_set_pixelformat(dither, pixelformat):
    _sixel.sixel_dither_set_pixelformat.restype = None
    _sixel.sixel_dither_set_pixelformat.argtypes = [c_void_p, c_int]
    _sixel.sixel_dither_set_pixelformat(dither, pixelformat)


def sixel_dither_set_transparent(dither, transparent):
    _sixel.sixel_dither_set_transparent.restype = None
    _sixel.sixel_dither_set_transparent.argtypes = [c_void_p, c_int]
    _sixel.sixel_dither_set_transparent(dither, transparent)


# convert pixels into sixel format and write it to output context
def sixel_encode(pixels, width, height, depth, dither, output):
    _sixel.sixel_encode.restype = c_int
    _sixel.sixel_encode.argtypes = [c_char_p, c_int, c_int, c_int, c_void_p, c_void_p]
    return _sixel.sixel_encode(pixels, width, height, depth, dither, output)


# create encoder object
def sixel_encoder_new(allocator=c_void_p(None)):
    _sixel.sixel_encoder_new.restype = c_int
    _sixel.sixel_encoder_new.argtypes = [POINTER(c_void_p), c_void_p]
    encoder = c_void_p(None)
    status = _sixel.sixel_encoder_new(byref(encoder), allocator)
    if SIXEL_FAILED(status):
        message = sixel_helper_format_error(status)
        raise RuntimeError(message)
    return encoder


# increase reference count of encoder object (thread-unsafe)
def sixel_encoder_ref(encoder):
    _sixel.sixel_encoder_ref.restype = None
    _sixel.sixel_encoder_ref.argtypes = [c_void_p]
    _sixel.sixel_encoder_ref(encoder)


# decrease reference count of encoder object (thread-unsafe)
def sixel_encoder_unref(encoder):
    _sixel.sixel_encoder_unref.restype = None
    _sixel.sixel_encoder_unref.argtypes = [c_void_p]
    _sixel.sixel_encoder_unref(encoder)


# set an option flag to encoder object
def sixel_encoder_setopt(encoder, flag, arg=None):
    _sixel.sixel_encoder_setopt.restype = c_int
    _sixel.sixel_encoder_setopt.argtypes = [c_void_p, c_int, c_char_p]
    flag = ord(flag)
    if arg:
        arg = str(arg).encode('utf-8')
    status = _sixel.sixel_encoder_setopt(encoder, flag, arg)
    if SIXEL_FAILED(status):
        message = sixel_helper_format_error(status)
        raise RuntimeError(message)


# load source data from specified file and encode it to SIXEL format
def sixel_encoder_encode(encoder, filename):
    import locale
    language, encoding = locale.getdefaultlocale()

    _sixel.sixel_encoder_encode.restype = c_int
    _sixel.sixel_encoder_encode.argtypes = [c_void_p, c_char_p]
    status = _sixel.sixel_encoder_encode(encoder, filename.encode(encoding))
    if SIXEL_FAILED(status):
        message = sixel_helper_format_error(status)
        raise RuntimeError(message)


# encode specified pixel data to SIXEL format
def sixel_encoder_encode_bytes(encoder, buf, width, height, pixelformat, palette):

    depth = sixel_helper_compute_depth(pixelformat)

    if depth <= 0:
        raise ValueError("invalid pixelformat value : %d" % pixelformat)

    if len(buf) < width * height * depth:
        raise ValueError("buf.len is too short : %d < %d * %d * %d" % (buf.len, width, height, depth))

    if not hasattr(buf, "readonly") or buf.readonly:
        cbuf = c_void_p.from_buffer_copy(buf)
    else:
        cbuf = c_void_p.from_buffer(buf)

    if palette:
        cpalettelen = len(palette)
        cpalette = (c_byte * cpalettelen)(*palette)
    else:
        cpalettelen = None
        cpalette = None

    _sixel.sixel_encoder_encode_bytes.restype = c_int
    _sixel.sixel_encoder_encode.argtypes = [c_void_p, c_void_p, c_int, c_int, c_int, c_void_p, c_int]

    status = _sixel.sixel_encoder_encode_bytes(encoder, buf, width, height, pixelformat, cpalette, cpalettelen)
    if SIXEL_FAILED(status):
        message = sixel_helper_format_error(status)
        raise RuntimeError(message)


# create decoder object
def sixel_decoder_new(allocator=c_void_p(None)):
    _sixel.sixel_decoder_new.restype = c_int
    _sixel.sixel_decoder_new.argtypes = [POINTER(c_void_p), c_void_p]
    decoder = c_void_p(None)
    status = _sixel.sixel_decoder_new(byref(decoder), c_void_p(None))
    if SIXEL_FAILED(status):
        message = sixel_helper_format_error(status)
        raise RuntimeError(message)
    return decoder


# increase reference count of decoder object (thread-unsafe)
def sixel_decoder_ref(decoder):
    _sixel.sixel_decoder_ref.restype = None
    _sixel.sixel_decoder_ref.argtypes = [c_void_p]
    _sixel.sixel_decoder_ref(decoder)


# decrease reference count of decoder object (thread-unsafe)
def sixel_decoder_unref(decoder):
    _sixel.sixel_decoder_unref.restype = None
    _sixel.sixel_decoder_unref.argtypes = [c_void_p]
    _sixel.sixel_decoder_unref(decoder)


# set an option flag to decoder object
def sixel_decoder_setopt(decoder, flag, arg=None):
    _sixel.sixel_decoder_setopt.restype = c_int
    _sixel.sixel_decoder_setopt.argtypes = [c_void_p, c_int, c_char_p]
    flag = ord(flag)
    if arg:
        arg = str(arg).encode('utf-8')
    status = _sixel.sixel_decoder_setopt(decoder, flag, arg)
    if SIXEL_FAILED(status):
        message = sixel_helper_format_error(status)
        raise RuntimeError(message)


# load source data from stdin or the file
def sixel_decoder_decode(decoder, infile=None):
    _sixel.sixel_decoder_decode.restype = c_int
    _sixel.sixel_decoder_decode.argtypes = [c_void_p]
    if infile:
        sixel_decoder_setopt(decoder, SIXEL_OPTFLAG_INPUT, infile)
    status = _sixel.sixel_decoder_decode(decoder)
    if SIXEL_FAILED(status):
        message = sixel_helper_format_error(status)
        raise RuntimeError(message)
