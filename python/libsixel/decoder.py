#!/usr/bin/env python
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
