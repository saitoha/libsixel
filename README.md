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
