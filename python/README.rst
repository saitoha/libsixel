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
    $ git checkout develop  # now available only develop branch
    $ ./configure --enable-python --prefix=/usr/local
    $ make install

Example 2. Install into only current active python ::

    $ git clone https://github.com/saitoha/libsixel.git
    $ cd libsixel 
    $ git checkout develop  # now available only develop branch
    $ ./configure --disable-python
    $ make install  # install libsixel
    $ cd python
    $ python setup.py install  # install python module


Code Example
------------

encoder ::

    from libsixel.encoder import Encoder

    encoder = Encoder()
    encoder.setopt("w", "300")
    encoder.setopt("p", "16")
    encoder.encode("test.png")


decoder ::

    from libsixel.decoder import Decoder

    decoder = Decoder()
    decoder.setopt("i", "test.six")
    decoder.setopt("o", "test.png")
    decoder.decode()
