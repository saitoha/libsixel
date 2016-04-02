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

class Decoder(object):

    def __init__(self):
        self._decoder = sixel_decoder_new()

    def __del__(self):
        sixel_decoder_unref(self._decoder)

    def setopt(self, flag, arg=None):
        sixel_decoder_setopt(self._decoder, flag, arg)

    def decode(self, infile=None):
        sixel_decoder_decode(self._decoder, infile)

    def test(self, infile=None, outfile=None):
        import threading

        if infile:
            self.setopt(SIXEL_OPTFLAG_INPUT, infile)
        if outfile:
            self.setopt(SIXEL_OPTFLAG_OUTPUT, outfile)
        t = threading.Thread(target=self.decode)
        t.daemon = True
        t.start()
        try:
            while t.is_alive():
                t.join(1)
        except KeyboardInterrupt:
            print("\033\\\033[Jcanceled.")


if __name__ == '__main__':
    import sys
    arg2 = "-" if len(sys.argv) < 3 else sys.argv[2]
    Decoder().test(arg1, arg2)
