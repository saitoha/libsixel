libsixel-python
===============

What is this?
-------------

This module is a python wrapper of libsixel.

libsixel: https://github.com/saitoha/libsixel

Install
-------

Example 1. Install into the python prefixed with '/usr/local' ::

    $ git clone https://github.com/saitoha/libsixel.git
    $ cd libsixel 
    $ ./configure --enable-python --prefix=/usr/local
    $ make install

Example 2. Install into only current active python ::

    $ git clone https://github.com/saitoha/libsixel.git
    $ cd libsixel 
    $ ./configure --disable-python
    $ make install  # install libsixel
    $ cd python
    $ python setup.py install  # install python module


Code Example
------------

encoder ::

    from libsixel.encoder import Encoder, SIXEL_OPTFLAG_WIDTH, SIXEL_OPTFLAG_COLORS

    encoder = Encoder()
    encoder.setopt(SIXEL_OPTFLAG_WIDTH, "300")
    encoder.setopt(SIXEL_OPTFLAG_COLORS, "16")
    encoder.encode("test.png")


decoder ::

    from libsixel.decoder import Decoder, SIXEL_OPTFLAG_INPUT, SIXEL_OPTFLAG_OUTPUT

    decoder = Decoder()
    decoder.setopt(SIXEL_OPTFLAG_INPUT, "test.six")
    decoder.setopt(SIXEL_OPTFLAG_OUTPUT, "test.png")
    # Request Kornel's undither without the refine pass:
    #
    #     +-------------+----------------------------------------+
    #     | k_undither  | Kornel's undither without refinement   |
    #     | k_undither+ | Kornel's undither with post-refine run |
    #     +-------------+----------------------------------------+
    #
    decoder.setopt(ord('d'), "k_undither")
    decoder.decode()


Float32 precision workflow
--------------------------

The Python bindings can drive the high precision pipeline introduced
for the CLI.  Keep the following sequence in mind when you want to
preserve float data end-to-end:

1. Build libsixel with the Python module enabled so that the wrapper
   uses the same shared library as `img2sixel`.
2. Opt into the float32 dithering path via the
   ``SIXEL_FLOAT32_DITHER`` environment variable before creating an
   encoder instance.  The value ``"1"`` matches ``img2sixel -.float32``.
3. When you feed your own buffers to ``encode_bytes`` you may keep them
   in one of the float pixel formats that libsixel exposes:

   * ``SIXEL_PIXELFORMAT_RGBFLOAT32`` for gamma-encoded RGB data.
   * ``SIXEL_PIXELFORMAT_LINEARRGBFLOAT32`` for linear-light RGB.
   * ``SIXEL_PIXELFORMAT_OKLABFLOAT32`` for working in OKLab.

   Passing any of these constants to ``encode_bytes`` avoids the
   implicit 8-bit conversion that older releases performed.
4. When libsixel loads the source image on your behalf, forward the
   precision flag through ``Encoder.setopt`` so the worker threads stay
   in the float pipeline:

   ::

       import os
       from libsixel.encoder import Encoder
       from libsixel import SIXEL_OPTFLAG_PRECISION

       os.environ["SIXEL_FLOAT32_DITHER"] = "1"

       encoder = Encoder()
       encoder.setopt(SIXEL_OPTFLAG_PRECISION, "float32")
       encoder.setopt(SIXEL_OPTFLAG_COLORS, "256")
       encoder.encode("images/autumn.png")

5. Combine the precision flag with the working colorspace option
   (``-W`` in the CLI, ``SIXEL_OPTFLAG_WORKING_COLORSPACE`` in Python)
   to convert into ``linear`` or ``oklab`` before quantization while the
   float buffers stay intact.


License
-------

The MIT lisence ::

    Copyright (c) 2014-2020 Hayaki Saito
    
    Permission is hereby granted, free of charge, to any person obtaining a copy of
    this software and associated documentation files (the "Software"), to deal in
    the Software without restriction, including without limitation the rights to
    use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of
    the Software, and to permit persons to whom the Software is furnished to do so,
    subject to the following conditions:
    
    The above copyright notice and this permission notice shall be included in all
    copies or substantial portions of the Software.
    
    THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
    IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS
    FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR
    COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER
    IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
    CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
