import unittest

try:
    import libsixel
    from libsixel.encoder import Encoder as PyEncoder
    _LIBSIXEL_LOADED = True
    _LIBSIXEL_LOAD_ERROR = None
except Exception as exc:  # pragma: no cover
    PyEncoder = None
    _LIBSIXEL_LOADED = False
    _LIBSIXEL_LOAD_ERROR = exc


class LibsixelBindingsTest(unittest.TestCase):
    @unittest.skipUnless(_LIBSIXEL_LOADED, "libsixel shared library not available: %s" % _LIBSIXEL_LOAD_ERROR)
    def test_optflags_exposed(self):
        self.assertTrue(hasattr(libsixel, 'SIXEL_OPTFLAG_DRCS'))
        self.assertTrue(hasattr(libsixel, 'SIXEL_OPTFLAG_ORMODE'))

    @unittest.skipUnless(_LIBSIXEL_LOADED, "libsixel shared library not available: %s" % _LIBSIXEL_LOAD_ERROR)
    def test_encoder_accepts_all_supported_flags(self):
        cases = [
            (libsixel.SIXEL_OPTFLAG_OUTFILE, '-'),
            (libsixel.SIXEL_OPTFLAG_7BIT_MODE, None),
            (libsixel.SIXEL_OPTFLAG_8BIT_MODE, None),
            (libsixel.SIXEL_OPTFLAG_HAS_GRI_ARG_LIMIT, None),
            (libsixel.SIXEL_OPTFLAG_COLORS, 16),
            (libsixel.SIXEL_OPTFLAG_MAPFILE, 'palette.map'),
            (libsixel.SIXEL_OPTFLAG_MONOCHROME, None),
            (libsixel.SIXEL_OPTFLAG_HIGH_COLOR, None),
            (libsixel.SIXEL_OPTFLAG_BUILTIN_PALETTE, 'xterm16'),
            (libsixel.SIXEL_OPTFLAG_DIFFUSION, 'atkinson'),
            (libsixel.SIXEL_OPTFLAG_FIND_LARGEST, 'norm'),
            (libsixel.SIXEL_OPTFLAG_SELECT_COLOR, 'center'),
            (libsixel.SIXEL_OPTFLAG_CROP, '10x10+0+0'),
            (libsixel.SIXEL_OPTFLAG_WIDTH, '80%'),
            (libsixel.SIXEL_OPTFLAG_HEIGHT, '120'),
            (libsixel.SIXEL_OPTFLAG_RESAMPLING, 'nearest'),
            (libsixel.SIXEL_OPTFLAG_QUALITY, 'high'),
            (libsixel.SIXEL_OPTFLAG_LOOPMODE, 'force'),
            (libsixel.SIXEL_OPTFLAG_PALETTE_TYPE, 'rgb'),
            (libsixel.SIXEL_OPTFLAG_ENCODE_POLICY, 'fast'),
            (libsixel.SIXEL_OPTFLAG_BGCOLOR, '#112233'),
            (libsixel.SIXEL_OPTFLAG_INSECURE, None),
            (libsixel.SIXEL_OPTFLAG_INVERT, None),
            (libsixel.SIXEL_OPTFLAG_USE_MACRO, None),
            (libsixel.SIXEL_OPTFLAG_MACRO_NUMBER, 1),
            (libsixel.SIXEL_OPTFLAG_COMPLEXION_SCORE, 2),
            (libsixel.SIXEL_OPTFLAG_IGNORE_DELAY, None),
            (libsixel.SIXEL_OPTFLAG_VERBOSE, None),
            (libsixel.SIXEL_OPTFLAG_STATIC, None),
            (libsixel.SIXEL_OPTFLAG_DRCS, None),
            (libsixel.SIXEL_OPTFLAG_PENETRATE, None),
            (libsixel.SIXEL_OPTFLAG_ORMODE, None),
            (libsixel.SIXEL_OPTFLAG_PIPE_MODE, None),
        ]

        for flag, value in cases:
            encoder = PyEncoder()
            if value is None:
                encoder.setopt(flag)
            else:
                encoder.setopt(flag, value)


if __name__ == '__main__':
    unittest.main()
