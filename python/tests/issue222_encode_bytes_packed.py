#!/usr/bin/env python
#
# Copyright (c) 2026 Hayaki Saito
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
# IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
# FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
# AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
# LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
# OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
# SOFTWARE.
#

import os
import sys
import ctypes
import ctypes.util


class FakeFunction(object):
    def __init__(self, callback):
        self.callback = callback
        self.argtypes = None
        self.restype = None

    def __call__(self, *args):
        return self.callback(*args)


class FakeSixel(object):
    def __init__(self):
        self.depths = {}
        self.encode_calls = []
        self.sixel_helper_compute_depth = FakeFunction(self.compute_depth)
        self.sixel_encoder_encode_bytes = FakeFunction(self.encode_bytes)

    def compute_depth(self, pixelformat):
        return self.depths.get(pixelformat, -1)

    def encode_bytes(self, encoder, buf, width, height, pixelformat,
                     palette, ncolors):
        self.encode_calls.append((encoder, buf, width, height, pixelformat,
                                  palette, ncolors))
        return 0


fake_sixel = FakeSixel()


# Stub the shared library loader so this wrapper validation test does not
# depend on an installed libsixel shared object.
def fake_find_library(name):
    if name != 'sixel':
        raise_assertion('unexpected library name')
    return 'libsixel-test-stub'


def fake_load_library(path):
    if path != 'libsixel-test-stub':
        raise_assertion('unexpected path')
    return fake_sixel


def raise_assertion(message):
    raise AssertionError(message)


ctypes.util.find_library = fake_find_library
ctypes.cdll.LoadLibrary = fake_load_library
sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.abspath(__file__))))

import libsixel

fake_sixel.depths.update({
    libsixel.SIXEL_PIXELFORMAT_RGB888: 3,
    libsixel.SIXEL_PIXELFORMAT_G1: 1,
    libsixel.SIXEL_PIXELFORMAT_G2: 1,
    libsixel.SIXEL_PIXELFORMAT_G4: 1,
    libsixel.SIXEL_PIXELFORMAT_PAL1: 1,
    libsixel.SIXEL_PIXELFORMAT_PAL2: 1,
    libsixel.SIXEL_PIXELFORMAT_PAL4: 1,
})

for pixelformat, width, height, expected in (
        (libsixel.SIXEL_PIXELFORMAT_G1, 17, 1, 3),
        (libsixel.SIXEL_PIXELFORMAT_G2, 5, 3, 6),
        (libsixel.SIXEL_PIXELFORMAT_G4, 3, 4, 8),
        (libsixel.SIXEL_PIXELFORMAT_PAL1, 17, 1, 3),
        (libsixel.SIXEL_PIXELFORMAT_PAL2, 5, 3, 6),
        (libsixel.SIXEL_PIXELFORMAT_PAL4, 3, 4, 8),
        (libsixel.SIXEL_PIXELFORMAT_RGB888, 2, 3, 18)):
    actual = libsixel._sixel_helper_compute_frame_size(pixelformat,
                                                       width,
                                                       height)
    if actual != expected:
        raise_assertion('unexpected frame size: %d != %d' %
                        (actual, expected))

libsixel.sixel_encoder_encode_bytes('encoder',
                                    '\xaa\x55\x80',
                                    17,
                                    1,
                                    libsixel.SIXEL_PIXELFORMAT_G1,
                                    None)

if len(fake_sixel.encode_calls) != 1:
    raise_assertion('packed G1 input was rejected before the C call')

try:
    libsixel.sixel_encoder_encode_bytes('encoder',
                                        '\xaa\x55',
                                        17,
                                        1,
                                        libsixel.SIXEL_PIXELFORMAT_G1,
                                        None)
except ValueError:
    exc = sys.exc_info()[1]
else:
    raise AssertionError('short packed G1 input was accepted')

if str(exc) != 'buf.len is too short : 2 < 3':
    raise_assertion('unexpected short G1 error: %s' % str(exc))

try:
    libsixel.sixel_encoder_encode_bytes('encoder',
                                        '\x00' * 17,
                                        2,
                                        3,
                                        libsixel.SIXEL_PIXELFORMAT_RGB888,
                                        None)
except ValueError:
    exc = sys.exc_info()[1]
else:
    raise AssertionError('short RGB888 input was accepted')

if str(exc) != 'buf.len is too short : 17 < 18':
    raise_assertion('unexpected short RGB888 error: %s' % str(exc))

libsixel.sixel_encoder_encode_bytes('encoder',
                                    '\x00' * 18,
                                    2,
                                    3,
                                    libsixel.SIXEL_PIXELFORMAT_RGB888,
                                    None)

try:
    libsixel.sixel_encoder_encode_bytes('encoder',
                                        '\x00',
                                        1,
                                        1,
                                        -1,
                                        None)
except ValueError:
    exc = sys.exc_info()[1]
else:
    raise AssertionError('invalid pixelformat was accepted')

if str(exc) != 'invalid pixelformat value : -1':
    raise_assertion('unexpected invalid pixelformat error: %s' % str(exc))
