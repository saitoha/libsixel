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

    // Common CLI-compatible option flags.
    public const SIXEL_OPTFLAG_INPUT = 'i';
    public const SIXEL_OPTFLAG_OUTPUT = 'o';
    public const SIXEL_OPTFLAG_OUTFILE = 'o';
    public const SIXEL_OPTFLAG_COLORS = 'p';
    public const SIXEL_OPTFLAG_WIDTH = 'w';
    public const SIXEL_OPTFLAG_HEIGHT = 'h';
    public const SIXEL_OPTFLAG_DIFFUSION = 'd';
    public const SIXEL_OPTFLAG_DEQUANTIZE = 'd';
    public const SIXEL_OPTFLAG_LOADERS = 'L';
    public const SIXEL_OPTFLAG_PRECISION = '.';
    public const SIXEL_OPTFLAG_THREADS = '=';
    public const SIXEL_OPTFLAG_WORKING_COLORSPACE = 'W';
}
