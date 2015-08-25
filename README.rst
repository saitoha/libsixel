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

