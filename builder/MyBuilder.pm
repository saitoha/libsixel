package MyBuilder;

use strict;
use warnings;
use parent qw(Module::Build);
use File::Copy;

sub new {
    my ($self, %args) = @_;
    $self->SUPER::new(
        %args,
        extra_compiler_flags => [scalar `libsixel-config --cflags`],
        extra_linker_flags => [scalar `libsixel-config --libs`],
    );
}

1;
