#!/usr/bin/env python
#
# Copyright (c) 2014-2020 Hayaki Saito
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

import glob
import locale
import os
import pathlib
import struct
from ctypes import (
    cdll,
    c_void_p,
    c_int,
    c_byte,
    c_char_p,
    POINTER,
    byref,
    CFUNCTYPE,
    string_at,
    cast,
)
from ctypes.util import find_library

# locale.getencoding() is available on newer Python versions. Fall back to
# getpreferredencoding() for Python 3.9/3.10 test environments.
def _resolve_locale_encoding(default="utf-8"):
    getencoding = getattr(locale, "getencoding", None)
    if callable(getencoding):
        encoding = getencoding()
    else:
        try:
            encoding = locale.getpreferredencoding(False)
        except TypeError:
            encoding = locale.getpreferredencoding()
    if not encoding:
        encoding = default
    return encoding

# limitations
SIXEL_OUTPUT_PACKET_SIZE     = 16384
SIXEL_PALETTE_MIN            = 2
SIXEL_PALETTE_MAX            = 256
SIXEL_USE_DEPRECATED_SYMBOLS = 1
SIXEL_ALLOCATE_BYTES_MAX     = 1024 * 1024 * 128  # up to 128M
SIXEL_WIDTH_LIMIT            = 1000000
SIXEL_HEIGHT_LIMIT           = 1000000

# loader settings
SIXEL_DEFALUT_GIF_DELAY      = 1

# loader option identifiers for sixel_loader_setopt().  The numeric values need
# to stay in sync with include/sixel.h.in so that Python callers can configure
# the loader object precisely.  Keeping the mapping here prevents mysterious
# breakage when new options are introduced in the C API.
#
#        +-------------------------------+
#        |  Python option -> C option    |
#        +-------------------------------+
#        | REQUIRE_STATIC  -> (1)        |
#        | USE_PALETTE     -> (2)        |
#        | REQCOLORS       -> (3)        |
#        | BGCOLOR         -> (4)        |
#        | LOOP_CONTROL    -> (5)        |
#        | INSECURE        -> (6)        |
#        | CANCEL_FLAG     -> (7)        |
#        | LOADER_ORDER    -> (8)        |
#        | CONTEXT         -> (9)        |
#        | WIC_ICO_MINSIZE -> (10)       |
#        | START_FRAME_NO  -> (11)       |
#        | BGCOLOR_SOURCE  -> (12)       |
#        +-------------------------------+
SIXEL_LOADER_OPTION_REQUIRE_STATIC = 1
SIXEL_LOADER_OPTION_USE_PALETTE = 2
SIXEL_LOADER_OPTION_REQCOLORS = 3
SIXEL_LOADER_OPTION_BGCOLOR = 4
SIXEL_LOADER_OPTION_LOOP_CONTROL = 5
SIXEL_LOADER_OPTION_INSECURE = 6
SIXEL_LOADER_OPTION_CANCEL_FLAG = 7
SIXEL_LOADER_OPTION_LOADER_ORDER = 8
SIXEL_LOADER_OPTION_CONTEXT = 9
SIXEL_LOADER_OPTION_WIC_ICO_MINSIZE = 10
SIXEL_LOADER_OPTION_START_FRAME_NO = 11
SIXEL_LOADER_OPTION_BGCOLOR_SOURCE = 12

SIXEL_LOADER_BGCOLOR_SOURCE_EXPLICIT = 0
SIXEL_LOADER_BGCOLOR_SOURCE_ENV = 1

# return value
SIXEL_OK              = 0x0000
SIXEL_FALSE           = 0x1000

# error codes
SIXEL_RUNTIME_ERROR        = (SIXEL_FALSE         | 0x0100)  # runtime error
SIXEL_LOGIC_ERROR          = (SIXEL_FALSE         | 0x0200)  # logic error
SIXEL_FEATURE_ERROR        = (SIXEL_FALSE         | 0x0300)  # feature not enabled
SIXEL_LIBC_ERROR           = (SIXEL_FALSE         | 0x0400)  # errors caused by curl
SIXEL_CURL_ERROR           = (SIXEL_FALSE         | 0x0500)  # errors occures in libc functions
SIXEL_JPEG_ERROR           = (SIXEL_FALSE         | 0x0600)  # errors occures in libjpeg functions
SIXEL_PNG_ERROR            = (SIXEL_FALSE         | 0x0700)  # errors occures in libpng functions
SIXEL_GDK_ERROR            = (SIXEL_FALSE         | 0x0800)  # errors occures in gdk functions
SIXEL_GD_ERROR             = (SIXEL_FALSE         | 0x0900)  # errors occures in gd functions
SIXEL_STBI_ERROR           = (SIXEL_FALSE         | 0x0a00)  # errors occures in stb_image functions
SIXEL_STBIW_ERROR          = (SIXEL_FALSE         | 0x0b00)  # errors occures in stb_image_write functions
SIXEL_COM_ERROR            = (SIXEL_FALSE         | 0x0c00)  # errors occures in COM functions
SIXEL_WIC_ERROR            = (SIXEL_FALSE         | 0x0d00)  # errors occures in WIC functions

SIXEL_INTERRUPTED          = (SIXEL_OK            | 0x0001)  # interrupted by a signal

SIXEL_BAD_ALLOCATION       = (SIXEL_RUNTIME_ERROR | 0x0001)  # malloc() failed
SIXEL_BAD_ARGUMENT         = (SIXEL_RUNTIME_ERROR | 0x0002)  # bad argument detected
SIXEL_BAD_INPUT            = (SIXEL_RUNTIME_ERROR | 0x0003)  # bad input detected
SIXEL_BAD_INTEGER_OVERFLOW = (SIXEL_RUNTIME_ERROR | 0x0004)  # integer overflow

SIXEL_NOT_IMPLEMENTED      = (SIXEL_FEATURE_ERROR | 0x0001)  # feature not implemented

def SIXEL_SUCCEEDED(status):
    return (((status) & 0x1000) == 0)

def SIXEL_FAILED(status):
    return (((status) & 0x1000) != 0)

# method for finding the largest dimension for splitting,
# and sorting by that component
SIXEL_LARGE_AUTO = 0x0   # choose automatically the method for finding the largest dimension
SIXEL_LARGE_NORM = 0x1   # simply comparing the range in RGB space
SIXEL_LARGE_LUM  = 0x2   # transforming into luminosities before the comparison
SIXEL_LARGE_PCA  = 0x3   # cut along the first principal component

# method for choosing a color from the box
SIXEL_REP_AUTO           = 0x0  # choose automatically the method for selecting representative color from each box
SIXEL_REP_CENTER_BOX     = 0x1  # choose the center of the box
SIXEL_REP_AVERAGE_COLORS = 0x2  # choose the average all the color in the box (specified in Heckbert's paper)
SIXEL_REP_AVERAGE_PIXELS = 0x3  # choose the average all the pixels in the box

# method for diffusing
SIXEL_DIFFUSE_AUTO         = 0x0  # choose diffusion type automatically
SIXEL_DIFFUSE_NONE         = 0x1  # don't diffuse
SIXEL_DIFFUSE_ATKINSON     = 0x2  # diffuse with Bill Atkinson's method
SIXEL_DIFFUSE_FS           = 0x3  # diffuse with Floyd-Steinberg method
SIXEL_DIFFUSE_JAJUNI       = 0x4  # diffuse with Jarvis, Judice & Ninke method
SIXEL_DIFFUSE_STUCKI       = 0x5  # diffuse with Stucki's method
SIXEL_DIFFUSE_BURKES       = 0x6  # diffuse with Burkes' method
SIXEL_DIFFUSE_A_DITHER     = 0x7  # positionally stable arithmetic dither
SIXEL_DIFFUSE_X_DITHER     = 0x8  # positionally stable arithmetic xor based dither
SIXEL_DIFFUSE_BLUENOISE_DITHER = 0x9  # positionally stable bluenoise dither
SIXEL_DIFFUSE_LSO2         = 0xa  # libsixel method based on variable error
                                  # diffusion
SIXEL_DIFFUSE_INTERFRAME   = 0xb  # interframe error diffusion
SIXEL_DIFFUSE_SIERRA1      = 0xc  # diffuse with Sierra Lite method
SIXEL_DIFFUSE_SIERRA2      = 0xd  # diffuse with Sierra Two-row method
SIXEL_DIFFUSE_SIERRA3      = 0xe  # diffuse with Sierra-3 method


_PYTHON_BITS = struct.calcsize("P") * 8


def _detect_elf_class(data: bytes) -> str:
    """Return the ELF class width as a string."""

    if not data.startswith(b"\x7fELF"):
        return ""

    elf_class = data[4]
    if elf_class == 1:
        return "32"
    if elf_class == 2:
        return "64"
    return ""


def _detect_macho_class(data: bytes) -> str:
    """Return the Mach-O class width as a string."""

    magic = data[:4]
    macho_32 = (0xfeedface, 0xcefaedfe)
    macho_64 = (0xfeedfacf, 0xcffaedfe)

    value = int.from_bytes(magic, byteorder="big", signed=False)
    if value in macho_32:
        return "32"
    if value in macho_64:
        return "64"
    return ""


def _detect_pe_class(bin_path: pathlib.Path, data: bytes) -> str:
    """Return the PE/COFF class width as a string."""

    if not data.startswith(b"MZ") or len(data) < 0x40:
        return ""

    pe_offset = int.from_bytes(data[0x3C:0x40], byteorder="little")
    try:
        with bin_path.open("rb") as handle:
            handle.seek(pe_offset)
            signature = handle.read(6)
    except OSError:
        return ""

    if not signature.startswith(b"PE\0\0") or len(signature) < 6:
        return ""

    machine = struct.unpack("<H", signature[4:6])[0]
    if machine in (0x014C,):
        return "32"
    if machine in (0x8664,):
        return "64"
    return ""


def _detect_library_bits(bin_path: pathlib.Path) -> str:
    """Inspect the binary header to determine its architecture width."""

    try:
        with bin_path.open("rb") as handle:
            head = handle.read(1024)
    except OSError:
        return ""

    for detector in (_detect_elf_class, _detect_macho_class):
        width = detector(head)
        if width:
            return width

    return _detect_pe_class(bin_path, head)
# scan order for diffusing
SIXEL_SCAN_AUTO       = 0x0  # choose scan order automatically
SIXEL_SCAN_RASTER     = 0x1  # scan from left to right on each line
SIXEL_SCAN_SERPENTINE = 0x2  # alternate scan direction per line

# quality modes
SIXEL_QUALITY_AUTO      = 0x0  # choose quality mode automatically
SIXEL_QUALITY_HIGH      = 0x1  # high quality palette construction
SIXEL_QUALITY_LOW       = 0x2  # low quality palette construction
SIXEL_QUALITY_FULL      = 0x3  # full quality palette construction
SIXEL_QUALITY_HIGHCOLOR = 0x4  # high color
SIXEL_QUANTIZE_MODEL_AUTO      = 0x0  # choose palette solver automatically
SIXEL_QUANTIZE_MODEL_MEDIANCUT = 0x1  # Heckbert median-cut solver
SIXEL_QUANTIZE_MODEL_KMEANS    = 0x2  # k-means palette solver
SIXEL_QUANTIZE_MODEL_KMEDOIDS  = 0x3  # k-medoids palette solver
SIXEL_FINAL_MERGE_AUTO         = 0x0  # select final merge automatically
                                      # (defaults to none)
SIXEL_FINAL_MERGE_NONE         = 0x1  # disable final merge stage
SIXEL_FINAL_MERGE_WARD         = 0x2  # Ward hierarchical clustering merge

# built-in dither
SIXEL_BUILTIN_MONO_DARK   = 0x0  # monochrome terminal with dark background
SIXEL_BUILTIN_MONO_LIGHT  = 0x1  # monochrome terminal with light background
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
SIXEL_PIXELFORMAT_ABGR8888 = (SIXEL_FORMATTYPE_COLOR     | 0x12) # 32bpp
SIXEL_PIXELFORMAT_BGRA8888 = (SIXEL_FORMATTYPE_COLOR     | 0x13) # 32bpp
SIXEL_PIXELFORMAT_RGBFLOAT32 = (SIXEL_FORMATTYPE_COLOR   | 0x20) # 96bpp float
SIXEL_PIXELFORMAT_LINEARRGBFLOAT32 = (SIXEL_FORMATTYPE_COLOR | 0x21)
SIXEL_PIXELFORMAT_OKLABFLOAT32 = (SIXEL_FORMATTYPE_COLOR | 0x22)
SIXEL_PIXELFORMAT_CIELABFLOAT32 = (SIXEL_FORMATTYPE_COLOR | 0x23)
SIXEL_PIXELFORMAT_DIN99DFLOAT32 = (SIXEL_FORMATTYPE_COLOR | 0x24)
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

# colorspace modes for clustering/working/output options
SIXEL_COLORSPACE_GAMMA  = 0x0  # gamma-encoded RGB
SIXEL_COLORSPACE_LINEAR = 0x1  # linear RGB
SIXEL_COLORSPACE_OKLAB  = 0x2  # OKLab
SIXEL_COLORSPACE_SMPTEC = 0x3  # SMPTE-C gamma
SIXEL_COLORSPACE_CIELAB = 0x4  # CIELAB
SIXEL_COLORSPACE_DIN99D = 0x5  # DIN99d

# palette type
SIXEL_PALETTETYPE_AUTO     = 0   # choose palette type automatically
SIXEL_PALETTETYPE_HLS      = 1   # HLS colorspace
SIXEL_PALETTETYPE_RGB      = 2   # RGB colorspace

# policies of SIXEL encoding
SIXEL_ENCODEPOLICY_AUTO    = 0   # choose encoding policy automatically
SIXEL_ENCODEPOLICY_FAST    = 1   # encode as fast as possible
SIXEL_ENCODEPOLICY_SIZE    = 2   # encode to as small sixel sequence as possible

# LUT policy constants mirror the C header so that Python callers can request
# the exact histogram backend they need.  Keeping the numeric values in sync is
# critical because the encoder forwards them directly to libsixel.
#
#   auto ----> channel depth based decision
#                |
#         +------+------+---------+
#         |      |       |
#      classic  none  certified
#      (5/6bit)        (certlut)
#
SIXEL_LUT_POLICY_AUTO      = 0x0  # choose LUT width automatically
SIXEL_LUT_POLICY_5BIT      = 0x1  # use legacy 5-bit buckets
SIXEL_LUT_POLICY_6BIT      = 0x2  # use 6-bit RGB buckets
SIXEL_LUT_POLICY_NONE      = 0x4  # disable LUT acceleration
SIXEL_LUT_POLICY_CERTLUT   = 0x5  # certified hierarchical LUT
SIXEL_LUT_POLICY_FHEDT      = 0x6  # Voronoi LUT with 3D EDT refinement
SIXEL_LUT_POLICY_EYTZINGER = 0x7  # Eytzinger implicit binary tree LUT
SIXEL_LUT_POLICY_VPTREE    = 0x8  # VP-tree palette lookup
SIXEL_LUT_POLICY_RBC       = 0x9  # randomized ball cover lookup
SIXEL_LUT_POLICY_MAHALANOBIS = 0xa  # Mahalanobis-aware lookup

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
SIXEL_OPTFLAG_HAS_GRI_ARG_LIMIT = 'R'  # -R, --gri-limit: clamp DECGRI arguments to 255.
SIXEL_OPTFLAG_PRECISION        = '.'  # -., --precision: control quantization precision.
SIXEL_OPTFLAG_THREADS          = '='  # -=, --threads: override encoder/decoder thread count.
SIXEL_OPTFLAG_LOADERS          = 'L'  # -L LIST, --loaders=LIST: override loader order (WIC: :ico_minsize=SIZE).
SIXEL_OPTFLAG_7BIT_MODE        = '7'  # -7, --7bit-mode: for 7bit terminals or printers (default)
SIXEL_OPTFLAG_8BIT_MODE        = '8'  # -8, --8bit-mode: for 8bit terminals or printers
SIXEL_OPTFLAG_6REVERSIBLE      = '6'  # -6, --6reversible: snap palette to reversible tones
SIXEL_OPTFLAG_COLORS           = 'p'  # -p COLORS, --colors=COLORS: specify number of colors
SIXEL_OPTFLAG_MAPFILE          = 'm'  # -m FILE, --mapfile=FILE: specify set of colors
SIXEL_OPTFLAG_MAPFILE_OUTPUT   = 'M'  # -M FILE, --mapfile-output=FILE: export palette file
SIXEL_OPTFLAG_MONOCHROME       = 'e'  # -e, --monochrome: output monochrome sixel image
SIXEL_OPTFLAG_INSECURE         = 'k'  # -k, --insecure: allow to connect to SSL sites without certs
SIXEL_OPTFLAG_INVERT           = 'i'  # -i, --invert: assume the terminal background color
SIXEL_OPTFLAG_HIGH_COLOR       = 'I'  # -I, --high-color: output 15bpp sixel image
SIXEL_OPTFLAG_USE_MACRO        = 'u'  # -u, --use-macro: use DECDMAC and DECINVM sequences
SIXEL_OPTFLAG_MACRO_NUMBER     = 'n'  # -n MACRONO, --macro-number=MACRONO:
                                      #        specify macro register number
SIXEL_OPTFLAG_COMPLEXION_SCORE = 'C'  # -C COMPLEXIONSCORE, --complexion-score=COMPLEXIONSCORE:
                                      #        (deprecated) specify an number argument for the
                                      #        score of complexion correction.
SIXEL_OPTFLAG_IGNORE_DELAY     = 'g'  # -g, --ignore-delay: render GIF animation without delay
SIXEL_OPTFLAG_STATIC           = 'S'  # -S, --static: render animated GIF as a static image
#
#   +------------+-------------------------------+
#   | short opt  | semantic scope                |
#   +------------+-------------------------------+
#   | -d         | decoder: dequantize palette   |
#   |            | encoder: diffusion selector   |
#   | -D         | decoder: emit RGBA (direct)   |
#   |            | encoder: legacy pipe-mode     |
#   +------------+-------------------------------+
#
# Python callers use these constants with ``Decoder.setopt``.  The table
# keeps the intent obvious when the same letter spans historic features.
SIXEL_OPTFLAG_DEQUANTIZE       = 'd'  # -d, --dequantize: repair palette.
SIXEL_OPTFLAG_DIRECT           = 'D'  # -D, --direct: decode to RGBA pixels.
SIXEL_OPTFLAG_SIMILARITY       = 'S'  # -S SCORE, --similarity-score=SCORE:
                                      #        set contour detector similarity
SIXEL_OPTFLAG_SIZE             = 's'  # -s SIZE, --segment-size=SIZE:
                                      #        set contour detector segment size
SIXEL_OPTFLAG_EDGE             = 'e'  # -e MODE, --detect-edge=MODE:
                                      #        set contour edge detector mode
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
                                      #            sierra1  -> Sierra Lite method
                                      #            sierra2  -> Sierra Two-row method
                                      #            sierra3  -> Sierra-3 method
                                      #            a_dither -> positionally stable
                                      #                        arithmetic dither
                                      #            x_dither -> positionally stable
                                      #                        arithmetic xor based dither
                                      #            lso2     -> libsixel method based on
                                      #                        variable error diffusion
                                      #                        + jitter
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
                                      #           pca  -> split along the first
                                      #                   principal component and
                                      #                   cut at weighted median

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
SIXEL_OPTFLAG_QUANTIZE_MODEL   = 'Q'  # -Q MODEL, --quantize-model=MODEL:
                                      #        choose the palette solver.
                                      #        MODEL is one of them:
                                      #          auto     -> select solver
                                      #                      automatically
                                      #                      (Heckbert)
                                      #          heckbert -> Heckbert median-cut
                                      #          kmeans   -> k-means palette
                                      #                      clustering
                                      #        kmeans accepts suboptions in:
                                      #          kmeans:key=value[:key=value...]
                                      #        supported suboptions:
                                      #          inittype (or i):
                                      #            auto, none, pca
                                      #          threshold (or t):
                                      #            float in 0.0-0.5
                                      #          binning (or b):
                                      #            auto, none, hard, soft
                                      #          binbits (or n):
                                      #            integer in 4-8
                                      #          mapping (or m):
                                      #            uniform, srgb
                                      #          softdist (or d):
                                      #            trilinear
                                      #          autoratio (or r):
                                      #            integer in 1-1048576
                                      #          feedback (or f):
                                      #            off, on
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
                                      #          <number>c  -> scale width with
                                      #                        terminal cell count
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
                                      #           <number>c  -> scale height with
                                      #                         terminal cell count
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

SIXEL_OPTFLAG_START_FRAME      = 'T'  # -T FRAME_NO, --start-frame=FRAME_NO:
                                      #        set the first animation frame index
                                      #          non-negative -> absolute index
                                      #          negative     -> offset from end

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
SIXEL_OPTFLAG_LUT_POLICY        = '~'  # -~ LOOKUPPOLICY,
                                      #   --lookup-policy=LOOKUPPOLICY:
                                      #        choose histogram lookup width.
                                      #          auto    -> follow pixel depth
                                      #          5bit    -> force 5-bit buckets
                                      #          6bit    -> force 6-bit buckets
                                      #                     (RGB inputs)
                                      #          none    -> disable LUT caching
                                      #                     and scan directly
                                      #          certlut -> certified
                                      #                     hierarchical LUT
                                      #                     with zero error
                                      #          fhedt    -> Voronoi grid built
                                      #                     via 3D EDT with
                                      #                     optional
                                      #                     refinement
SIXEL_OPTFLAG_CLUSTERING_COLORSPACE = 'X'  # -X COLORSPACE, --clustering-colorspace=COLORSPACE:
                                          #        select palette clustering space.
                                          #          gamma  -> keep gamma encoded pixels
                                          #          linear -> convert to linear RGB
                                          #          oklab  -> operate in OKLab
                                          #          cielab -> operate in CIELAB
                                          #          din99d -> operate in DIN99d
SIXEL_OPTFLAG_WORKING_COLORSPACE = 'W'  # -W WORKING_COLORSPACE, --working-colorspace=COLORSPACE:
                                      #        select internal working space.
                                      #          gamma  -> keep gamma encoded pixels
                                      #          linear -> convert to linear RGB
                                      #          oklab  -> operate in OKLab
                                      #          cielab -> operate in CIELAB
                                      #          din99d -> operate in DIN99d
SIXEL_OPTFLAG_OUTPUT_COLORSPACE = 'U'  # -U OUTPUT_COLORSPACE, --output-colorspace=COLORSPACE:
                                      #        select output buffer color space.
                                      #          gamma   -> sRGB gamma encoded output
                                      #          linear  -> linear RGB output
                                      #          smpte-c -> SMPTE-C gamma encoded output
SIXEL_OPTFLAG_ORMODE           = 'O'  # -O, --ormode: output ormode sixel image

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

SIXEL_OPTFLAG_PENETRATE        = 'P'  # -P, --penetrate: (deprecated)
                                      #        penetrate GNU Screen using DCS
                                      #        pass-through sequence
SIXEL_OPTFLAG_DRCS             = '@'  # -@ MMV:CHARSET:PATH, --drcs=MMV:CHARSET:PATH:
                                      #        emit extended DRCS tiles, optionally
                                      #        overriding mapping revision, charset,
                                      #        and tile sink (defaults to 2:1:;
                                      #        experimental)
SIXEL_OPTFLAG_PIPE_MODE        = 'D'  # -D, --pipe-mode: (deprecated)
                                      #         read source images from stdin continuously
SIXEL_OPTFLAG_VERBOSE          = 'v'  # -v, --verbose: show debugging info
SIXEL_OPTFLAG_VERSION          = 'V'  # -V, --version: show version and license info
SIXEL_OPTFLAG_HELP             = 'H'  # -H, --help: show this help

_sixel_names = [
    "sixel",
    "libsixel",
    "sixel-1",
    "libsixel-1",
    "msys-sixel",
    "cygsixel",
]

def _match_library_in_dir(libdir, lib_names):
    """Return the first matching shared library in the given directory.

    The selection logic is intentionally aligned with the build helper:

    - Accept both "lib" prefixed and prefixless names.
    - Accept .so, .dylib, or .dll (including versioned .so.* files).
    - Skip import archives like *.dll.a or *.dll.def.
    """

    prefixes = ["lib", ""]
    suffixes = [".so", ".dylib", ".dll"]

    for name in lib_names:
        for prefix in prefixes:
            for suffix in suffixes:
                patterns: list[str]
                if suffix == ".so":
                    patterns = [
                        os.path.join(libdir, f"{prefix}{name}*{suffix}"),
                        os.path.join(libdir, f"{prefix}{name}*{suffix}.*"),
                    ]
                else:
                    patterns = [
                        os.path.join(libdir, f"{prefix}{name}*{suffix}"),
                    ]

                matches: list[str] = []
                for pattern in patterns:
                    matches.extend(glob.glob(pattern))

                filtered: list[str] = []
                for candidate in sorted(set(matches)):
                    if candidate.endswith((".dll.a", ".dll.def")):
                        continue

                    bits = _detect_library_bits(pathlib.Path(candidate))
                    if bits and bits != str(_PYTHON_BITS):
                        continue

                    filtered.append(candidate)

                if filtered:
                    return filtered[0]

    return None


def _prefer_bundled_library(lib_names):
    """Locate a bundled shared library shipped inside the wheel package.

    Wheel builds copy the shared library into libsixel/_libs so that imports
    succeed even when libsixel is not installed system-wide.
    """

    bundle_dir = pathlib.Path(__file__).resolve().parent / "_libs"
    if not bundle_dir.is_dir():
        return None
    return _match_library_in_dir(str(bundle_dir), lib_names)


def _prefer_env_library(lib_names):
    """Locate libsixel under LIBSIXEL_LIBDIR when running from a build tree."""

    libdir = os.environ.get("LIBSIXEL_LIBDIR")
    if libdir is None:
        return None
    return _match_library_in_dir(libdir, lib_names)


_lib_path = _prefer_bundled_library(_sixel_names)

if _lib_path is None:
    _lib_path = _prefer_env_library(_sixel_names)

if _lib_path is None:
    _lib_path = next(
        (path for path in (find_library(name) for name in _sixel_names)
         if path is not None),
        None,
    )

if _lib_path is None:
    raise ImportError(
        "libsixel not found. Set LIBSIXEL_LIBDIR to the built shared library."
    )

_lib_bits = _detect_library_bits(pathlib.Path(_lib_path))
if _lib_bits and _lib_bits != str(_PYTHON_BITS):
    raise ImportError(
        f"libsixel {_lib_bits}-bit library is incompatible with "
        f"python {_PYTHON_BITS}-bit"
    )

# load shared library
_sixel = cdll.LoadLibrary(_lib_path)

# convert error status code int formatted string
def sixel_helper_format_error(status):
    _sixel.sixel_helper_format_error.restype = c_char_p;
    _sixel.sixel_helper_format_error.argtypes = [c_int];
    return _sixel.sixel_helper_format_error(status)


# compute pixel depth from pixelformat
def sixel_helper_compute_depth(pixelformat):
    _sixel.sixel_helper_compute_depth.restype = c_int
    _sixel.sixel_helper_compute_depth.argtypes = [c_int]
    return _sixel.sixel_helper_compute_depth(pixelformat)


# generic loader -----------------------------------------------------------

_sixel_loader_callback_type = CFUNCTYPE(c_int, c_void_p, c_void_p)


def sixel_loader_new(allocator=c_void_p(None)):
    """Create a loader object that mirrors sixel_loader_new()."""

    _sixel.sixel_loader_new.restype = c_int
    _sixel.sixel_loader_new.argtypes = [POINTER(c_void_p), c_void_p]

    loader = c_void_p(None)
    status = _sixel.sixel_loader_new(byref(loader), allocator)
    if SIXEL_FAILED(status):
        message = sixel_helper_format_error(status)
        raise RuntimeError(message)
    return loader


def sixel_loader_ref(loader):
    """Increase the reference count of a loader object."""

    _sixel.sixel_loader_ref.restype = None
    _sixel.sixel_loader_ref.argtypes = [c_void_p]
    _sixel.sixel_loader_ref(loader)


def sixel_loader_unref(loader):
    """Decrease the reference count of a loader object."""

    _sixel.sixel_loader_unref.restype = None
    _sixel.sixel_loader_unref.argtypes = [c_void_p]
    _sixel.sixel_loader_unref(loader)


def sixel_loader_setopt(loader, option, value=None):
    """Configure loader behavior via sixel_loader_setopt().

    The helper routes Python values into the pointer-based C API while keeping
    the conversion rules in plain sight:

        +-----------+---------------------------+---------------------+
        | Option    | Expected Python value     | Example             |
        +-----------+---------------------------+---------------------+
        | STATIC    | bool/int or None          | True                |
        | PALETTE   | bool/int or None          | 0                   |
        | REQCOLORS | int or None               | 256                 |
        | BGCOLOR   | iterable[3] or None       | (0, 0, 0)           |
        | LOOP      | int or None               | SIXEL_LOOP_FORCE    |
        | INSECURE  | bool/int or None          | False               |
        | CANCEL    | ctypes pointer / address  | byref(c_int(0))     |
        | ORDER     | str/bytes/bytearray or None | "builtin"         |
        | CONTEXT   | ctypes pointer / address  | c_void_p(id(obj))   |
        | WIC SIZE  | int or None               | 64                  |
        +-----------+---------------------------+---------------------+

    Values left as ``None`` map to NULL so that the C side may install its
    default behavior.
    """

    _sixel.sixel_loader_setopt.restype = c_int
    _sixel.sixel_loader_setopt.argtypes = [c_void_p, c_int, c_void_p]

    option = int(option)
    pointer_value = c_void_p(None)
    keepalive = None

    int_options = {
        SIXEL_LOADER_OPTION_REQUIRE_STATIC,
        SIXEL_LOADER_OPTION_USE_PALETTE,
        SIXEL_LOADER_OPTION_REQCOLORS,
        SIXEL_LOADER_OPTION_LOOP_CONTROL,
        SIXEL_LOADER_OPTION_INSECURE,
        SIXEL_LOADER_OPTION_WIC_ICO_MINSIZE,
        SIXEL_LOADER_OPTION_START_FRAME_NO,
    }

    if option in int_options:
        if value is not None:
            keepalive = c_int(int(value))
            pointer_value = cast(byref(keepalive), c_void_p)
    elif option == SIXEL_LOADER_OPTION_BGCOLOR:
        if value is not None:
            if len(value) != 3:
                raise ValueError("bgcolor expects three components")
            keepalive = (c_byte * 3)(value[0], value[1], value[2])
            pointer_value = cast(keepalive, c_void_p)
    elif option == SIXEL_LOADER_OPTION_LOADER_ORDER:
        if value is not None:
            if isinstance(value, bytes):
                encoded = value
            elif isinstance(value, bytearray):
                encoded = bytes(value)
            elif isinstance(value, str):
                encoded = value.encode('utf-8')
            else:
                raise TypeError(
                    "loader_order expects str, bytes, bytearray, or None"
                )
            keepalive = c_char_p(encoded)
            pointer_value = cast(keepalive, c_void_p)
    elif option in (
        SIXEL_LOADER_OPTION_CANCEL_FLAG,
        SIXEL_LOADER_OPTION_CONTEXT,
    ):
        if value is None:
            pointer_value = c_void_p(None)
        elif isinstance(value, c_void_p):
            pointer_value = value
        elif isinstance(value, int):
            pointer_value = c_void_p(value)
        else:
            pointer_value = cast(value, c_void_p)
    else:
        raise ValueError("unknown loader option: %r" % option)

    status = _sixel.sixel_loader_setopt(loader, option, pointer_value)
    if SIXEL_FAILED(status):
        message = sixel_helper_format_error(status)
        raise RuntimeError(message)


def sixel_loader_load_file(loader, filename, fn_load):
    """Load ``filename`` and feed each frame to ``fn_load``.

    ``fn_load`` receives ``(frame_ptr, context_ptr)`` mirroring the C
    signature.  The loader's context pointer may be set via
    ``sixel_loader_setopt``.
    """

    if fn_load is None:
        raise ValueError("fn_load callback is required")
    if not callable(fn_load):
        raise TypeError("fn_load callback must be callable")

    _sixel.sixel_loader_load_file.restype = c_int
    _sixel.sixel_loader_load_file.argtypes = [
        c_void_p,
        c_char_p,
        _sixel_loader_callback_type,
    ]

    encoding = _resolve_locale_encoding(default="utf-8")

    # The C API expects a non-NULL filename pointer.
    # Reject None in the Python wrapper to avoid passing NULL and
    # crashing inside the native loader implementation.
    if filename is None:
        raise TypeError("filename must be str or bytes, not None")
    elif isinstance(filename, bytes):
        c_filename = filename
    else:
        c_filename = filename.encode(encoding)

    def _fn_load_local(frame, context):
        return fn_load(frame, context)

    callback = _sixel_loader_callback_type(_fn_load_local)
    status = _sixel.sixel_loader_load_file(loader, c_filename, callback)
    if SIXEL_FAILED(status):
        message = sixel_helper_format_error(status)
        raise RuntimeError(message)


# create new output context object
def sixel_output_new(fn_write, priv=None, allocator=c_void_p(None)):
    output = c_void_p(None)

    # ctypes callback exceptions do not propagate to the original Python
    # caller. Keep the original exception object on the output handle so
    # sixel_encode() can re-raise it in the caller context.
    output.__callback_exception = None

    def _fn_write_local(data, size, priv_from_c):
        try:
            fn_write(string_at(data, size), priv)
        except Exception as exc:
            output.__callback_exception = exc
            return -1
        return size

    sixel_write_function = CFUNCTYPE(c_int, c_char_p, c_int, c_void_p)
    _sixel.sixel_output_new.restype = c_int
    _sixel.sixel_output_new.argtypes = [POINTER(c_void_p), sixel_write_function, c_void_p, c_void_p]
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
    output.__callback_exception = None


# get 8bit output mode which indicates whether it uses C1 control characters
def sixel_output_get_8bit_availability(output):
    _sixel.sixel_output_get_8bit_availability.restype = c_int
    _sixel.sixel_output_get_8bit_availability.argtypes = [c_void_p]
    return _sixel.sixel_output_get_8bit_availability(output)


# set 8bit output mode state
def sixel_output_set_8bit_availability(output, availability):
    _sixel.sixel_output_set_8bit_availability.restype = None
    _sixel.sixel_output_set_8bit_availability.argtypes = [c_void_p, c_int]
    _sixel.sixel_output_set_8bit_availability(output, availability)


# set whether limit arguments of DECGRI('!') to 255
def sixel_output_set_gri_arg_limit(output, value):
    _sixel.sixel_output_set_gri_arg_limit.restype = None
    _sixel.sixel_output_set_gri_arg_limit.argtypes = [c_void_p, c_int]
    _sixel.sixel_output_set_gri_arg_limit(output, value)


# set GNU Screen penetration feature enable or disable
def sixel_output_set_penetrate_multiplexer(output, penetrate):
    _sixel.sixel_output_set_penetrate_multiplexer.restype = None
    _sixel.sixel_output_set_penetrate_multiplexer.argtypes = [c_void_p, c_int]
    _sixel.sixel_output_set_penetrate_multiplexer(output, penetrate)


# set whether we skip DCS envelope
def sixel_output_set_skip_dcs_envelope(output, skip):
    _sixel.sixel_output_set_skip_dcs_envelope.restype = None
    _sixel.sixel_output_set_skip_dcs_envelope.argtypes = [c_void_p, c_int]
    _sixel.sixel_output_set_skip_dcs_envelope(output, skip)


# set whether we skip SIXEL header
def sixel_output_set_skip_header(output, skip):
    _sixel.sixel_output_set_skip_header.restype = None
    _sixel.sixel_output_set_skip_header.argtypes = [c_void_p, c_int]
    _sixel.sixel_output_set_skip_header(output, skip)


# set palette type: RGB or HLS
def sixel_output_set_palette_type(output, palettetype):
    _sixel.sixel_output_set_palette_type.restype = None
    _sixel.sixel_output_set_palette_type.argtypes = [c_void_p, c_int]
    _sixel.sixel_output_set_palette_type(output, palettetype)


# enable or disable ormode output
def sixel_output_set_ormode(output, ormode):
    _sixel.sixel_output_set_ormode.restype = None
    _sixel.sixel_output_set_ormode.argtypes = [c_void_p, c_int]
    _sixel.sixel_output_set_ormode(output, ormode)


# set encodeing policy: auto, fast or size
def sixel_output_set_encode_policy(output, encode_policy):
    _sixel.sixel_output_set_encode_policy.restype = None
    _sixel.sixel_output_set_encode_policy.argtypes = [c_void_p, c_int]
    _sixel.sixel_output_set_encode_policy(output, encode_policy)


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


def sixel_dither_set_diffusion_scan(dither, method_for_scan):
    _sixel.sixel_dither_set_diffusion_scan.restype = None
    _sixel.sixel_dither_set_diffusion_scan.argtypes = [c_void_p, c_int]
    _sixel.sixel_dither_set_diffusion_scan(dither, method_for_scan)


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
    return list(cpalette)


def sixel_dither_set_palette(dither, palette):
    _sixel.sixel_dither_set_palette.restype = None
    _sixel.sixel_dither_set_palette.argtypes = [c_void_p, c_char_p]
    cpalette = bytes(palette)
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


# configure the encoder thread count for band parallelism
def sixel_set_threads(threads):
    auto_requested = False
    value = 0
    text = None

    if isinstance(threads, bytes):
        try:
            text = threads.decode('utf-8').strip()
        except UnicodeDecodeError as exc:
            raise ValueError(
                "threads must be a positive integer or 'auto'"
            ) from exc
    elif isinstance(threads, str):
        text = threads.strip()
    else:
        text = None

    if text is not None:
        if text.lower() == 'auto':
            auto_requested = True
            value = 0
        else:
            try:
                value = int(text, 10)
            except ValueError as exc:
                raise ValueError(
                    "threads must be a positive integer or 'auto'"
                ) from exc
    else:
        try:
            value = int(threads)
        except (TypeError, ValueError) as exc:
            raise ValueError(
                "threads must be a positive integer or 'auto'"
            ) from exc

    if auto_requested is False and value < 1:
        raise ValueError("threads must be a positive integer or 'auto'")

    _sixel.sixel_set_threads.restype = None
    _sixel.sixel_set_threads.argtypes = [c_int]
    _sixel.sixel_set_threads(value)


# convert pixels into sixel format and write it to output context
def sixel_encode(pixels, width, height, depth, dither, output):
    _sixel.sixel_encode.restype = c_int
    _sixel.sixel_encode.argtypes = [c_char_p, c_int, c_int, c_int, c_void_p, c_void_p]
    status = _sixel.sixel_encode(pixels, width, height, depth, dither, output)

    callback_exception = getattr(output, '__callback_exception', None)
    if callback_exception is not None:
        output.__callback_exception = None
        raise callback_exception

    return status


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
    # Normalize flag for validation while keeping the numeric code used by the
    # C API. Python callers may pass either the character constant ("p") or an
    # integer value. We want to keep the original character for comparison so
    # option-specific validation continues to work even after converting to the
    # numeric code for ctypes.
    if isinstance(flag, int):
        flag_code = flag
        flag_char = chr(flag)
    else:
        flag_char = str(flag)
        if len(flag_char) != 1:
            raise RuntimeError(
                "invalid option flag: expected a single-character flag"
            )
        flag_code = ord(flag_char)

    if arg:
        arg = str(arg).encode('utf-8')
    status = _sixel.sixel_encoder_setopt(encoder, flag_code, arg)
    if SIXEL_FAILED(status):
        message = sixel_helper_format_error(status)
        raise RuntimeError(message)


# load source data from specified file and encode it to SIXEL format
def sixel_encoder_encode(encoder, filename):
    import os
    encoding = _resolve_locale_encoding(default="ascii")

    # Reject None before touching the codec path because the C API expects a
    # real string pointer. This keeps the exception class deterministic and
    # mirrors the explicit None guard used by sixel_loader_load_file().
    if filename is None:
        raise TypeError("filename must be str or bytes, not None")
    if isinstance(filename, memoryview):
        raise TypeError("filename must be str or bytes, not memoryview")

    # Proactively validate the input path on the Python side so callers get a
    # deterministic exception even if a platform-specific libc or loader fails
    # to surface a failure.  This mirrors the C-side validation while keeping
    # the behaviour consistent across wheel and in-tree builds.
    if isinstance(filename, bytes):
        encoded_filename = filename
        stdin_token = b"-"
    else:
        encoded_filename = str(filename).encode(encoding)
        stdin_token = b"-"

    if encoded_filename != stdin_token:
        if not os.path.exists(filename):
            raise RuntimeError(f"input path does not exist: {filename}")
        if os.path.isdir(filename):
            raise RuntimeError(f"input path is a directory: {filename}")

    _sixel.sixel_encoder_encode.restype = c_int
    _sixel.sixel_encoder_encode.argtypes = [c_void_p, c_char_p]
    status = _sixel.sixel_encoder_encode(encoder, encoded_filename)
    if SIXEL_FAILED(status):
        message = sixel_helper_format_error(status)
        raise RuntimeError(message)


# encode specified pixel data to SIXEL format
def sixel_encoder_encode_bytes(encoder, buf, width, height, pixelformat, palette):

    depth = sixel_helper_compute_depth(pixelformat)

    if depth <= 0:
        raise ValueError("invalid pixelformat value : %d" % pixelformat)

    if not isinstance(buf, bytes):
        raise TypeError("buf must be bytes")

    if len(buf) < width * height * depth:
        raise ValueError("buf.len is too short : %d < %d * %d * %d" % (len(buf), width, height, depth))

    if palette:
        cpalettelen = len(palette)
        cpalette = (c_byte * cpalettelen)(*palette)
    else:
        cpalettelen = 0
        cpalette = None

    _sixel.sixel_encoder_encode_bytes.restype = c_int
    _sixel.sixel_encoder_encode_bytes.argtypes = [c_void_p, c_void_p, c_int, c_int, c_int, c_void_p, c_int]

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
