# NAME

Image::LibSIXEL - The Perl interface for libsixel (A lightweight, fast implementation of DEC SIXEL graphics codec)

# SYNOPSIS

        use Image::LibSIXEL;
        
        $encoder = Image::LibSIXEL::Encoder->new();
        $encoder->setopt("w", 400);
        $encoder->setopt("p", 16);
        $encoder->encode("images/egret.jpg");
        
        $decoder = Image::LibSIXEL::Decoder->new();
        $decoder->setopt("i", "images/egret.six");
        $decoder->setopt("o", "egret.png");
        $decoder->decode();

# DESCRIPTION

This perl module provides wrapper objects for part of libsixel interface.
http://saitoha.github.io/libsixel/

# LICENSE

Copyright (c) 2014,2015 Hayaki Saito

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

# DESCRIPTION


## Class Methods

&#x3d;Image::LibSIXEL::Encoder->new

Create Encoder object

&#x3d;Image::LibSIXEL::Decoder->new

Create Decoder object

## Object Methods

&#x3d;Image::LibSIXEL::Encoder->setopt

&#x3d;Image::LibSIXEL::Encoder->encode

&#x3d;Image::LibSIXEL::Decoder->setopt

&#x3d;Image::LibSIXEL::Decoder->decode

# AUTHOR

Hayaki Saito <saitoha@me.com>

# SEE ALSO
