package Image::LibSIXEL;
use 5.012001;
our $VERSION = '0.01';

require XSLoader;
XSLoader::load('Image::LibSIXEL', $VERSION);

1;

__END__

=head1 NAME

Image::LibSIXEL - The Perl interface for libsixel (A lightweight, fast implementation of DEC SIXEL graphics codec)

=head1 SYNOPSIS

	use Image::LibSIXEL;
	
	$encoder = Image::LibSIXEL::Encoder->new();
	$encoder->setopt("w", 400);
	$encoder->setopt("p", 16);
	$encoder->encode("images/egret.jpg");
	
	$decoder = Image::LibSIXEL::Decoder->new();
	$decoder->setopt("i", "images/egret.six");
	$decoder->setopt("o", "egret.png");
	$decoder->decode();

=head1 DESCRIPTION

This perl module provides wrapper objects for part of libsixel interface.
http://saitoha.github.io/libsixel/

=head2 Class Methods

=Image::LibSIXEL::Encoder->new

Create Encoder object

=Image::LibSIXEL::Decoder->new

Create Decoder object

=head2 Object Methods

=Image::LibSIXEL::Encoder->setopt

=Image::LibSIXEL::Encoder->encode

=Image::LibSIXEL::Decoder->setopt

=Image::LibSIXEL::Decoder->decode

=head1 AUTHOR

Hayaki Saito <saitoha@me.com>

=head1 SEE ALSO

=cut
