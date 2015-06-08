#!/usr/bin/env python
#
# Copyright (c) 2014,2015 Hayaki Saito
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

class Decoder(object):

    def __init__(self):
        from ctypes import c_void_p, c_int, c_char_p

        _sixel.sixel_decoder_create.restype = c_void_p
        _sixel.sixel_decoder_unref.restype = None
        _sixel.sixel_decoder_unref.argtypes = [c_void_p]
        _sixel.sixel_decoder_setopt.argtypes = [c_void_p, c_int, c_char_p]
        _sixel.sixel_decoder_decode.argtypes = [c_void_p]
        self._decoder = _sixel.sixel_decoder_create()

    def __del__(self):
        _sixel.sixel_decoder_unref(self._decoder)

    def setopt(self, flag, arg=None):
        flag = ord(flag)
        if arg:
            arg = str(arg)
        settings = self._decoder
        result = _sixel.sixel_decoder_setopt(settings, flag, arg)
        if result != 0:
            raise RuntimeError("Invalid option was set.")

    def decode(self, infile=None):
        if infile:
            self.setopt("i", infile)
        result = _sixel.sixel_decoder_decode(self._decoder)
        if result != 0:
            raise RuntimeError("Unexpected Error")

    def test(self, infile=None, outfile=None):
        import threading

        if infile:
            self.setopt("i", infile)
        if outfile:
            self.setopt("o", outfile)
        t = threading.Thread(target=self.decode)
        t.daemon = True
        t.start()
        try:
            while t.is_alive():
                t.join(1)
        except KeyboardInterrupt as e:
            print "\033\\\033[Jcanceled."


if __name__ == '__main__':
    import sys
    arg2 = "-" if len(sys.argv) < 3 else sys.argv[2]
    Decoder().test(arg1, arg2)
