# Before 'make install' is performed this script should be runnable with
# 'make test'. After 'make install' it should work as 'perl Hello.t'

#########################

# change 'tests => 1' to 'tests => last_test_to_print';

use strict;
use warnings;
use Test::More;

#use Test::More;
BEGIN { use_ok('Image::LibSIXEL') };
BEGIN { use_ok('Image::LibSIXEL::Constants') };
BEGIN {
  unlink "egret.six", "egret.png";
};

#########################

# Insert your test code below, the Test::More module is use()ed here so read
# its man page ( perldoc Test::More ) for help writing this test script.


subtest 'api surface' => sub {
  can_ok 'Image::LibSIXEL',
    'sixel_loader_new',
    'sixel_loader_load_file',
    'sixel_loader_setopt',
    'sixel_output_new',
    'sixel_dither_new',
    'sixel_dither_initialize',
    'sixel_frame_new',
    'sixel_set_threads',
    'sixel_helper_compute_depth';
};

subtest 'encoder' => sub {
  use Image::LibSIXEL;
  my $encoder = Image::LibSIXEL::Encoder->new();
  isa_ok $encoder, 'Image::LibSIXEL::Encoder';
  can_ok $encoder, 'setopt', 'encode';
  $encoder->setopt("o", "egret.six");
  $encoder->setopt("w", "200");
  $encoder->encode("images/egret.jpg");
  ok -f "egret.six", 'output file exists';
};


subtest 'constants sync' => sub {
  my $header = '../include/sixel.h.in';
  ok -f $header, 'public header exists';

  open my $fh, '<', $header or die $!;
  my @missing;
  while (my $line = <$fh>) {
    next if $line !~ /^#define\s+(SIXEL_OPTFLAG_[A-Z0-9_]+)\s+/;
    my $name = $1;
    push @missing, $name if !Image::LibSIXEL::Constants->can($name);
  }
  close $fh;

  is_deeply \@missing, [], 'optflag constants follow public header';
};

subtest 'decoder' => sub {
  use Image::LibSIXEL;
  my $decoder = Image::LibSIXEL::Decoder->new;
  isa_ok $decoder, 'Image::LibSIXEL::Decoder';
  can_ok $decoder, 'setopt', 'decode';
  $decoder->setopt("i", "images/egret.six");
  $decoder->setopt("o", "egret.png");
  $decoder->decode();
  ok -f "egret.png", 'output file exists';
};

done_testing;
