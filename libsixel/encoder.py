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
from libsixel import *

class Encoder(object):

    def __init__(self):
        self._encoder = sixel_encoder_new()

    def __del__(self):
        sixel_encoder_unref(self._encoder)

    def setopt(self, flag, arg=None):
        sixel_encoder_setopt(self._encoder, flag, arg)

    def encode(self, filename="-"):
        sixel_encoder_encode(self._encoder, filename)

    def encode_bytes(self, buf, width, height, pixelformat, palette):
        sixel_encoder_encode_bytes(self._encoder, buf, width, height, pixelformat, palette)

    def test(self, filename):
        import threading

        self.setopt(SIXEL_OPTFLAG_COLORS, 16)
        self.setopt(SIXEL_OPTFLAG_DIFFUSION, "atkinson")
        self.setopt(SIXEL_OPTFLAG_WIDTH, 200)
        t = threading.Thread(target=self.encode, args=[filename])
        t.daemon = True
        t.start()
        try:
            while t.is_alive():
                t.join(1)
        except KeyboardInterrupt:
            print("\033\\\033[Jcanceled.")


if __name__ == '__main__':
    import sys
    arg1 = "-" if len(sys.argv) < 2 else sys.argv[1]
    Encoder().test(arg1)
