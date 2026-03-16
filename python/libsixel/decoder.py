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

from . import _sixel
from libsixel_wheel import *

class Decoder(object):

    def __init__(self):
        self._decoder = sixel_decoder_new()
        self._closed = False

    def close(self):
        if self._closed:
            return

        sixel_decoder_unref(self._decoder)
        self._closed = True

    def __enter__(self):
        return self

    def __exit__(self, exc_type, exc_value, traceback):
        self.close()

    def __del__(self):
        try:
            self.close()
        except Exception:
            # Avoid raising from __del__ to keep interpreter shutdown quiet.
            pass

    def setopt(self, flag, arg=None):
        """Set decoder options mirroring the sixel2png CLI flags.

        The dequantizer switch understands the Kornel preset shown
        below so callers can enable palette reconstruction explicitly:

            +-------------+---------------------------+
            | k_undither  | Kornel's undither filter  |
            +-------------+---------------------------+

        Pass the string as ``arg`` alongside the ``'d'`` flag to
        activate the smoothing step during decode.
        """
        if self._closed:
            raise RuntimeError("decoder has been closed")
        sixel_decoder_setopt(self._decoder, flag, arg)

    def decode(self, infile=None):
        if self._closed:
            raise RuntimeError("decoder has been closed")
        sixel_decoder_decode(self._decoder, infile)


if __name__ == '__main__':
    import sys
    arg2 = "-" if len(sys.argv) < 3 else sys.argv[2]
    Decoder().test(arg1, arg2)
