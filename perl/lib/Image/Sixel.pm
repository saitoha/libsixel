package Image::Sixel;
use 5.012001;
our $VERSION = '0.0_1';

require XSLoader;
XSLoader::load('Image::Sixel', $VERSION);

1;

__END__

=head1 NAME

Image::Sixel - The Perl interface for libsixel (A lightweight, fast implementation of DEC SIXEL graphics codec)

=head1 SYNOPSIS

	use Image::Sixel;
	
	$encoder = Image::Sixel::Encoder->new();
	$encoder->setopt("w", 400);
	$encoder->setopt("p", 16);
	$encoder->encode("images/egret.jpg");
	
	$decoder = Image::Sixel::Decoder->new();
	$decoder->setopt("i", "images/egret.six");
	$decoder->setopt("o", "egret.png");
	$decoder->decode();

=head1 DESCRIPTION

This perl module provides wrapper objects for part of libsixel interface.
http://saitoha.github.io/libsixel/

=head2 Class Methods

=Image::Sixel::Encoder->new

Create Encoder object

=Image::Sixel::Decoder->new

Create Decoder object

=head2 Object Methods

=Image::Sixel::Encoder->setopt

=Image::Sixel::Encoder->encode

=Image::Sixel::Decoder->setopt

=Image::Sixel::Decoder->decode

=head1 AUTHOR

Hayaki Saito <saitoha@me.com>

=head1 SEE ALSO

=cut
