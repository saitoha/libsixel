#!/usr/bin/env python
#
# This code is in public domain
#

from ctypes import *
import ctypes.util

def _load():
    sixel = CDLL(ctypes.util.find_library("sixel"))
    sixel.sixel_encoder_create.restype = c_void_p
    sixel.sixel_encoder_unref.restype = None
    sixel.sixel_encoder_unref.argtypes = [c_void_p]
    sixel.sixel_encoder_setopt.argtypes = [c_void_p, c_int, c_char_p]
    sixel.sixel_encoder_encode.argtypes = [c_void_p, c_char_p]
    sixel.sixel_decoder_create.restype = c_void_p
    sixel.sixel_decoder_unref.restype = None
    sixel.sixel_decoder_unref.argtypes = [c_void_p]
    sixel.sixel_decoder_setopt.argtypes = [c_void_p, c_int, c_char_p]
    sixel.sixel_decoder_decode.argtypes = [c_void_p]

    return sixel


class EasyEncoder(object):

    def __init__(self):
        self._sixel = _load()
        self._encoder = self._sixel.sixel_encoder_create()

    def __del__(self):
        self._sixel.sixel_encoder_unref(self._encoder)

    def setopt(self, flag, arg=None):
        flag = ord(flag)
        if arg:
            arg = str(arg)
        settings = self._encoder
        result = self._sixel.sixel_encoder_setopt(settings, flag, arg)
        if result != 0:
            raise RuntimeError("Invalid option was set.")

    def encode(self, filename="-"):
        result = self._sixel.sixel_encoder_encode(self._encoder, filename)
        if result != 0:
            raise RuntimeError("Unexpected Error")

    def test(self, filename):
        import threading

        self.setopt("p", 16)
        self.setopt("d", "atkinson")
        self.setopt("w", "200")
        t = threading.Thread(target=self.encode, args=[filename])
        t.daemon = True
        t.start()
        try:
            while t.is_alive():
                t.join(1)
        except KeyboardInterrupt as e:
            print "\033\\\033[Jcanceled."


class EasyDecoder(object):

    def __init__(self):
        self._sixel = _load()
        self._decoder = self._sixel.sixel_decoder_create()

    def __del__(self):
        self._sixel.sixel_decoder_unref(self._decoder)

    def setopt(self, flag, arg=None):
        flag = ord(flag)
        if arg:
            arg = str(arg)
        settings = self._decoder
        result = self._sixel.sixel_decoder_setopt(settings, flag, arg)
        if result != 0:
            raise RuntimeError("Invalid option was set.")

    def decode(self, infile=None):
        if infile:
            self.setopt("i", infile)
        result = self._sixel.sixel_decoder_decode(self._decoder)
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

    arg1 = "-" if len(sys.argv) < 2 else sys.argv[1]
    EasyEncoder().test(arg1)
#    arg2 = "-" if len(sys.argv) < 3 else sys.argv[2]
#    EasyDecoder().test(arg1, arg2)

