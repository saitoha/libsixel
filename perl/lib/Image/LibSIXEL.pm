package Image::LibSIXEL;
use 5.012001;
use strict;
use warnings;

use Carp qw(croak);
use FFI::Platypus 2.00;
use File::Basename qw(dirname);
use File::Spec;
use Image::LibSIXEL::Constants ();
use Image::LibSIXEL::GeneratedAttach ();

our $VERSION = '0.01';

# Keep one shared FFI object and expose APIs with a granularity similar to
# the Python and Ruby bindings.  The low-level functions are attached once
# and thin Perl wrappers provide status checks and callback lifetime control.
my $ffi = FFI::Platypus->new(api => 2);
my $module_dir = File::Spec->catdir(dirname(__FILE__), 'LibSIXEL');
my @bundled_candidates = (
    File::Spec->catfile($module_dir, 'libsixel.so.1'),
    File::Spec->catfile($module_dir, 'libsixel.so'),
    File::Spec->catfile($module_dir, 'libsixel.dylib'),
    File::Spec->catfile($module_dir, 'libsixel.dll'),
);
my ($bundled_lib) = grep { -f $_ } @bundled_candidates;

defined $bundled_lib
    or croak 'Bundled libsixel shared library is missing. '
           . 'Reinstall Image::LibSIXEL to rebuild it.';
$ffi->lib($bundled_lib);

Image::LibSIXEL::GeneratedAttach::attach_all($ffi);

# sixel_set_threads is currently exported from the shared library but not
# declared in include/sixel.h(.in), so keep this one explicit attach.
$ffi->attach(['sixel_set_threads' => 'Image::LibSIXEL::sixel_set_threads']
    => ['int'] => 'void');

# Keep callback closure references to avoid premature garbage collection while
# C-side objects still own and invoke callback pointers.
my %output_callback_keepalive;
my %loader_callback_keepalive;

sub _croak_on_status {
    my ($status) = @_;

    return if $status == Image::LibSIXEL::Constants::SIXEL_OK();
    croak sprintf 'libsixel error: %s', _sixel_helper_format_error($status);
}

sub _validate_setopt {
    my ($opt) = @_;

    defined $opt || croak 'Bad argument: undefined option';
    length $opt == 1 || croak sprintf 'Bad argument: %s', $opt;
    return ord($opt);
}

sub sixel_loader_new {
    my ($allocator) = @_;
    my $loader;
    my $status;

    $status = _sixel_loader_new(\$loader, $allocator);
    _croak_on_status($status);
    return $loader;
}

sub sixel_loader_load_file {
    my ($loader, $filename, $cb) = @_;
    my $status;
    my $closure;

    ref($cb) eq 'CODE' || croak 'callback must be CODE';
    $closure = sub {
        my ($frame, $context) = @_;
        return $cb->($frame, $context);
    };
    $loader_callback_keepalive{$loader + 0} = $closure;
    $status = _sixel_loader_load_file($loader, $filename, $closure);
    _croak_on_status($status);
    return;
}

sub sixel_loader_setopt {
    my ($loader, $option, $value) = @_;
    my $status;

    $status = _sixel_loader_setopt($loader, $option, $value);
    _croak_on_status($status);
    return;
}

sub sixel_output_new {
    my ($cb, $priv, $allocator) = @_;
    my $output;
    my $status;
    my $closure;

    ref($cb) eq 'CODE' || croak 'callback must be CODE';
    $closure = sub {
        my ($data_ptr, $size, $priv_from_c) = @_;
        my $payload;

        $payload = '';
        if (defined $data_ptr && $size > 0) {
            $payload = $ffi->cast('opaque', 'string', $data_ptr);
            if (defined $payload && length($payload) > $size) {
                $payload = substr($payload, 0, $size);
            }
        }
        return $cb->($payload, $size, $priv // $priv_from_c);
    };

    $status = _sixel_output_new(\$output, $closure, $priv, $allocator);
    _croak_on_status($status);
    $output_callback_keepalive{$output + 0} = $closure;
    return $output;
}

sub sixel_dither_new {
    my ($ncolors, $allocator) = @_;
    my $dither;
    my $status;

    $status = _sixel_dither_new(\$dither, $ncolors, $allocator);
    _croak_on_status($status);
    return $dither;
}

sub sixel_dither_initialize {
    my ($dither, $data, $width, $height, $pixelformat,
        $method_for_largest, $method_for_rep, $quality_mode) = @_;
    my $status;

    $status = _sixel_dither_initialize($dither, $data, $width, $height,
                                       $pixelformat, $method_for_largest,
                                       $method_for_rep, $quality_mode);
    _croak_on_status($status);
    return;
}

sub sixel_frame_new {
    my ($allocator) = @_;
    my $frame;
    my $status;

    $status = _sixel_frame_new(\$frame, $allocator);
    _croak_on_status($status);
    return $frame;
}

package Image::LibSIXEL::Encoder;

sub new {
    my ($class) = @_;
    my $ptr;
    my $status;

    $status = Image::LibSIXEL::_sixel_encoder_new(\$ptr, undef);
    Image::LibSIXEL::_croak_on_status($status);
    return bless { ptr => $ptr }, $class;
}

sub setopt {
    my ($self, $opt, $optarg) = @_;
    my $status;
    my $ch;

    ref $self || Carp::croak 'Bad invocant';
    $ch = Image::LibSIXEL::_validate_setopt($opt);
    $status = Image::LibSIXEL::_sixel_encoder_setopt($self->{ptr}, $ch, $optarg);
    Image::LibSIXEL::_croak_on_status($status);
    return;
}

sub encode {
    my ($self, $infile) = @_;
    my $status;

    ref $self || Carp::croak 'Bad invocant';
    defined $infile || Carp::croak 'Bad argument: undefined input file';
    $status = Image::LibSIXEL::_sixel_encoder_encode($self->{ptr}, $infile);
    Image::LibSIXEL::_croak_on_status($status);
    return;
}

sub DESTROY {
    my ($self) = @_;

    return if !defined $self->{ptr};
    Image::LibSIXEL::_sixel_encoder_unref($self->{ptr});
    delete $self->{ptr};
    return;
}

package Image::LibSIXEL::Decoder;

sub new {
    my ($class) = @_;
    my $ptr;
    my $status;

    $status = Image::LibSIXEL::_sixel_decoder_new(\$ptr, undef);
    Image::LibSIXEL::_croak_on_status($status);
    return bless { ptr => $ptr }, $class;
}

sub setopt {
    my ($self, $opt, $optarg) = @_;
    my $status;
    my $ch;

    ref $self || Carp::croak 'Bad invocant';
    $ch = Image::LibSIXEL::_validate_setopt($opt);
    $status = Image::LibSIXEL::_sixel_decoder_setopt($self->{ptr}, $ch, $optarg);
    Image::LibSIXEL::_croak_on_status($status);
    return;
}

sub decode {
    my ($self) = @_;
    my $status;

    ref $self || Carp::croak 'Bad invocant';
    $status = Image::LibSIXEL::_sixel_decoder_decode($self->{ptr});
    Image::LibSIXEL::_croak_on_status($status);
    return;
}

sub DESTROY {
    my ($self) = @_;

    return if !defined $self->{ptr};
    Image::LibSIXEL::_sixel_decoder_unref($self->{ptr});
    delete $self->{ptr};
    return;
}

1;

__END__

=head1 NAME

Image::LibSIXEL - The Perl interface for libsixel (A lightweight, fast
implementation of DEC SIXEL graphics codec)

=head1 SYNOPSIS

    use Image::LibSIXEL;

    my $encoder = Image::LibSIXEL::Encoder->new();
    $encoder->setopt('w', 400);
    $encoder->setopt('p', 16);
    $encoder->encode('images/egret.jpg');

    my $decoder = Image::LibSIXEL::Decoder->new();
    $decoder->setopt('i', 'images/egret.six');
    $decoder->setopt('o', 'egret.png');
    $decoder->decode();

=head1 DESCRIPTION

This module provides a thin object-oriented wrapper for selected libsixel APIs.

=head1 CONSTANTS

Constants defined in C public headers are available from
C<Image::LibSIXEL::Constants>.

=head1 AUTHOR

Hayaki Saito E<lt>saitoha@me.comE<gt>

=cut
