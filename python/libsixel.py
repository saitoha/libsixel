#!/usr/bin/env python
#
# This code is in public domain
#

from ctypes import *
import ctypes.util

def _load():
    sixel = CDLL(ctypes.util.find_library("sixel"))
    sixel.sixel_encode_settings_create.restype = c_void_p
    sixel.sixel_encode_settings_unref.restype = None
    sixel.sixel_encode_settings_unref.argtypes = [c_void_p]
    sixel.sixel_easy_encode_setopt.argtypes = [c_void_p, c_int, c_char_p]
    sixel.sixel_easy_encode.argtypes = [c_char_p, c_void_p, c_void_p]
    sixel.sixel_decode_settings_create.restype = c_void_p
    sixel.sixel_decode_settings_unref.restype = None
    sixel.sixel_decode_settings_unref.argtypes = [c_void_p]
    sixel.sixel_easy_decode_setopt.argtypes = [c_void_p, c_int, c_char_p]
    sixel.sixel_easy_decode.argtypes = [c_void_p]

    return sixel


class EasyEncoder(object):

    def __init__(self):
        self._sixel = _load()
        self._settings = self._sixel.sixel_encode_settings_create()

    def __del__(self):
        self._sixel.sixel_encode_settings_unref(self._settings)

    def setopt(self, flag, arg=None):
        flag = ord(flag)
        if arg:
            arg = str(arg)
        settings = self._settings
        result = self._sixel.sixel_easy_encode_setopt(settings, flag, arg)
        if result != 0:
            raise RuntimeError("Invalid option was set.")

    def encode(self, filename="-"):
        result = self._sixel.sixel_easy_encode(filename, self._settings, None)
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
        self._settings = self._sixel.sixel_decode_settings_create()

    def __del__(self):
        self._sixel.sixel_decode_settings_unref(self._settings)

    def setopt(self, flag, arg=None):
        flag = ord(flag)
        if arg:
            arg = str(arg)
        settings = self._settings
        result = self._sixel.sixel_easy_decode_setopt(settings, flag, arg)
        if result != 0:
            raise RuntimeError("Invalid option was set.")

    def decode(self, infile=None):
        if infile:
            self.setopt("i", infile)
        result = self._sixel.sixel_easy_decode(self._settings)
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
#    arg2 = "-" if len(sys.argv) < 3 else sys.argv[2]
    EasyEncoder().test(arg1)
#    EasyDecoder().test(arg1, arg2)

