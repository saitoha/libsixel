<?php
/**
 * High-level constants for libsixel PHP bindings.
 *
 * @license MIT
 */

declare(strict_types=1);

namespace Libsixel;

final class Constants
{
    private function __construct()
    {
    }

    // Status helpers
    public const SIXEL_OK = 0x0000;
    public const SIXEL_FALSE = 0x1000;

    public const SIXEL_RUNTIME_ERROR = 0x1100;
    public const SIXEL_LOGIC_ERROR = 0x1200;
    public const SIXEL_FEATURE_ERROR = 0x1300;
    public const SIXEL_BAD_ALLOCATION = 0x1101;
    public const SIXEL_BAD_ARGUMENT = 0x1102;
    public const SIXEL_BAD_INPUT = 0x1103;

    // Quantization and palette selection sub-options.
    public const SIXEL_LARGE_AUTO = 0x0;
    public const SIXEL_LARGE_NORM = 0x1;
    public const SIXEL_LARGE_LUM = 0x2;
    public const SIXEL_LARGE_PCA = 0x3;

    public const SIXEL_REP_AUTO = 0x0;
    public const SIXEL_REP_CENTER_BOX = 0x1;
    public const SIXEL_REP_AVERAGE_COLORS = 0x2;
    public const SIXEL_REP_AVERAGE_PIXELS = 0x3;

    public const SIXEL_DIFFUSE_AUTO = 0x0;
    public const SIXEL_DIFFUSE_NONE = 0x1;
    public const SIXEL_DIFFUSE_ATKINSON = 0x2;
    public const SIXEL_DIFFUSE_FS = 0x3;
    public const SIXEL_DIFFUSE_JAJUNI = 0x4;
    public const SIXEL_DIFFUSE_STUCKI = 0x5;
    public const SIXEL_DIFFUSE_BURKES = 0x6;
    public const SIXEL_DIFFUSE_A_DITHER = 0x7;
    public const SIXEL_DIFFUSE_X_DITHER = 0x8;
    public const SIXEL_DIFFUSE_BLUENOISE_DITHER = 0x9;
    public const SIXEL_DIFFUSE_LSO2 = 0xa;
    public const SIXEL_DIFFUSE_SIERRA1 = 0xc;
    public const SIXEL_DIFFUSE_SIERRA2 = 0xd;
    public const SIXEL_DIFFUSE_SIERRA3 = 0xe;

    public const SIXEL_SCAN_AUTO = 0x0;
    public const SIXEL_SCAN_RASTER = 0x1;
    public const SIXEL_SCAN_SERPENTINE = 0x2;

    public const SIXEL_CARRY_AUTO = 0x0;
    public const SIXEL_CARRY_DISABLE = 0x1;
    public const SIXEL_CARRY_ENABLE = 0x2;

    public const SIXEL_QUALITY_AUTO = 0x0;
    public const SIXEL_QUALITY_HIGH = 0x1;
    public const SIXEL_QUALITY_LOW = 0x2;
    public const SIXEL_QUALITY_FULL = 0x3;
    public const SIXEL_QUALITY_HIGHCOLOR = 0x4;

    public const SIXEL_LUT_POLICY_AUTO = 0x0;
    public const SIXEL_LUT_POLICY_5BIT = 0x1;
    public const SIXEL_LUT_POLICY_6BIT = 0x2;
    public const SIXEL_LUT_POLICY_NONE = 0x4;
    public const SIXEL_LUT_POLICY_CERTLUT = 0x5;
    public const SIXEL_LUT_POLICY_FHEDT = 0x6;
    public const SIXEL_LUT_POLICY_EYTZINGER = 0x7;
    public const SIXEL_LUT_POLICY_VPTREE = 0x8;
    public const SIXEL_LUT_POLICY_RBC = 0x9;
    public const SIXEL_LUT_POLICY_MAHALANOBIS = 0xa;

    public const SIXEL_COLORSPACE_GAMMA = 0x0;
    public const SIXEL_COLORSPACE_LINEAR = 0x1;
    public const SIXEL_COLORSPACE_OKLAB = 0x2;
    public const SIXEL_COLORSPACE_SMPTEC = 0x3;
    public const SIXEL_COLORSPACE_CIELAB = 0x4;
    public const SIXEL_COLORSPACE_DIN99D = 0x5;

    public const SIXEL_QUANTIZE_MODEL_AUTO = 0x0;
    public const SIXEL_QUANTIZE_MODEL_MEDIANCUT = 0x1;
    public const SIXEL_QUANTIZE_MODEL_KMEANS = 0x2;

    public const SIXEL_FINAL_MERGE_AUTO = 0x0;
    public const SIXEL_FINAL_MERGE_NONE = 0x1;
    public const SIXEL_FINAL_MERGE_WARD = 0x2;

    public const SIXEL_PALETTETYPE_AUTO = 0;
    public const SIXEL_PALETTETYPE_HLS = 1;
    public const SIXEL_PALETTETYPE_RGB = 2;

    public const SIXEL_ENCODEPOLICY_AUTO = 0;
    public const SIXEL_ENCODEPOLICY_FAST = 1;
    public const SIXEL_ENCODEPOLICY_SIZE = 2;

    public const SIXEL_RES_NEAREST = 0;
    public const SIXEL_RES_GAUSSIAN = 1;
    public const SIXEL_RES_HANNING = 2;
    public const SIXEL_RES_HAMMING = 3;
    public const SIXEL_RES_BILINEAR = 4;
    public const SIXEL_RES_WELSH = 5;
    public const SIXEL_RES_BICUBIC = 6;
    public const SIXEL_RES_LANCZOS2 = 7;
    public const SIXEL_RES_LANCZOS3 = 8;
    public const SIXEL_RES_LANCZOS4 = 9;

    public const SIXEL_LOOP_AUTO = 0;
    public const SIXEL_LOOP_FORCE = 1;
    public const SIXEL_LOOP_DISABLE = 2;

    public const SIXEL_BUILTIN_MONO_DARK = 0x0;
    public const SIXEL_BUILTIN_MONO_LIGHT = 0x1;
    public const SIXEL_BUILTIN_XTERM16 = 0x2;
    public const SIXEL_BUILTIN_XTERM256 = 0x3;
    public const SIXEL_BUILTIN_VT340_MONO = 0x4;
    public const SIXEL_BUILTIN_VT340_COLOR = 0x5;
    public const SIXEL_BUILTIN_G1 = 0x6;
    public const SIXEL_BUILTIN_G2 = 0x7;
    public const SIXEL_BUILTIN_G4 = 0x8;
    public const SIXEL_BUILTIN_G8 = 0x9;

    // setopt option flags.
    public const SIXEL_OPTFLAG_INPUT = 'i';
    public const SIXEL_OPTFLAG_OUTPUT = 'o';
    public const SIXEL_OPTFLAG_DEQUANTIZE = 'd';
    public const SIXEL_OPTFLAG_DIRECT = 'D';
    public const SIXEL_OPTFLAG_SIMILARITY = 'S';
    public const SIXEL_OPTFLAG_SIZE = 's';
    public const SIXEL_OPTFLAG_EDGE = 'e';
    public const SIXEL_OPTFLAG_OUTFILE = 'o';
    public const SIXEL_OPTFLAG_7BIT_MODE = '7';
    public const SIXEL_OPTFLAG_8BIT_MODE = '8';
    public const SIXEL_OPTFLAG_6REVERSIBLE = '6';
    public const SIXEL_OPTFLAG_HAS_GRI_ARG_LIMIT = 'R';
    public const SIXEL_OPTFLAG_PRECISION = '.';
    public const SIXEL_OPTFLAG_THREADS = '=';
    public const SIXEL_OPTFLAG_COLORS = 'p';
    public const SIXEL_OPTFLAG_MAPFILE = 'm';
    public const SIXEL_OPTFLAG_MAPFILE_OUTPUT = 'M';
    public const SIXEL_OPTFLAG_MONOCHROME = 'e';
    public const SIXEL_OPTFLAG_INSECURE = 'k';
    public const SIXEL_OPTFLAG_INVERT = 'i';
    public const SIXEL_OPTFLAG_HIGH_COLOR = 'I';
    public const SIXEL_OPTFLAG_USE_MACRO = 'u';
    public const SIXEL_OPTFLAG_MACRO_NUMBER = 'n';
    public const SIXEL_OPTFLAG_COMPLEXION_SCORE = 'C';
    public const SIXEL_OPTFLAG_IGNORE_DELAY = 'g';
    public const SIXEL_OPTFLAG_STATIC = 'S';
    public const SIXEL_OPTFLAG_DIFFUSION = 'd';
    public const SIXEL_OPTFLAG_DIFFUSION_SCAN = 'y';
    public const SIXEL_OPTFLAG_DIFFUSION_CARRY = 'Y';
    public const SIXEL_OPTFLAG_FIND_LARGEST = 'f';
    public const SIXEL_OPTFLAG_SELECT_COLOR = 's';
    public const SIXEL_OPTFLAG_QUANTIZE_MODEL = 'Q';
    public const SIXEL_OPTFLAG_FINAL_MERGE = 'F';
    public const SIXEL_OPTFLAG_CROP = 'c';
    public const SIXEL_OPTFLAG_WIDTH = 'w';
    public const SIXEL_OPTFLAG_HEIGHT = 'h';
    public const SIXEL_OPTFLAG_RESAMPLING = 'r';
    public const SIXEL_OPTFLAG_QUALITY = 'q';
    public const SIXEL_OPTFLAG_LOOPMODE = 'l';
    public const SIXEL_OPTFLAG_START_FRAME = 'T';
    public const SIXEL_OPTFLAG_PALETTE_TYPE = 't';
    public const SIXEL_OPTFLAG_BUILTIN_PALETTE = 'b';
    public const SIXEL_OPTFLAG_ENCODE_POLICY = 'E';
    public const SIXEL_OPTFLAG_LUT_POLICY = '~';
    public const SIXEL_OPTFLAG_CLUSTERING_COLORSPACE = 'X';
    public const SIXEL_OPTFLAG_WORKING_COLORSPACE = 'W';
    public const SIXEL_OPTFLAG_OUTPUT_COLORSPACE = 'U';
    public const SIXEL_OPTFLAG_ORMODE = 'O';
    public const SIXEL_OPTFLAG_BGCOLOR = 'B';
    public const SIXEL_OPTFLAG_PENETRATE = 'P';
    public const SIXEL_OPTFLAG_DRCS = '@';
    public const SIXEL_OPTFLAG_PIPE_MODE = 'D';
    public const SIXEL_OPTFLAG_VERBOSE = 'v';
    public const SIXEL_OPTFLAG_LOADERS = 'L';
    public const SIXEL_OPTFLAG_VERSION = 'V';
    public const SIXEL_OPTFLAG_HELP = 'H';

    // Loader option identifiers.
    public const SIXEL_LOADER_OPTION_REQUIRE_STATIC = 1;
    public const SIXEL_LOADER_OPTION_USE_PALETTE = 2;
    public const SIXEL_LOADER_OPTION_REQCOLORS = 3;
    public const SIXEL_LOADER_OPTION_BGCOLOR = 4;
    public const SIXEL_LOADER_OPTION_LOOP_CONTROL = 5;
    public const SIXEL_LOADER_OPTION_INSECURE = 6;
    public const SIXEL_LOADER_OPTION_CANCEL_FLAG = 7;
    public const SIXEL_LOADER_OPTION_LOADER_ORDER = 8;
    public const SIXEL_LOADER_OPTION_CONTEXT = 9;
    public const SIXEL_LOADER_OPTION_WIC_ICO_MINSIZE = 10;
    public const SIXEL_LOADER_OPTION_START_FRAME_NO = 11;
}
