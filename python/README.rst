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
    decoder.decode()


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
