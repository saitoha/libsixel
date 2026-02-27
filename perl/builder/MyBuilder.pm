package MyBuilder;

use strict;
use warnings;
use parent qw(Module::Build);
use Cwd qw(abs_path);
use File::Basename qw(basename);
use File::Copy qw(copy);
use File::Path qw(make_path);
use File::Spec;

sub _run_or_die {
    my ($self, @cmd) = @_;

    system(@cmd) == 0
        or die sprintf "command failed (%d): %s\n", ($? >> 8), join(' ', @cmd);
}

sub _paths {
    my ($self) = @_;
    my %paths;

    $paths{repo_root} = abs_path(File::Spec->catdir($self->base_dir, '..'));
    $paths{build_dir} = File::Spec->catdir($self->base_dir, '_libsixel-build');
    $paths{prefix_dir} = File::Spec->catdir($self->base_dir, '_libsixel-prefix');
    $paths{stage_lib_dir} = File::Spec->catdir($self->blib, 'lib', 'Image',
                                               'LibSIXEL');
    return \%paths;
}

sub _configured_marker {
    my ($self) = @_;
    my $paths;

    $paths = $self->_paths();
    return File::Spec->catfile($paths->{build_dir}, '.configured');
}

sub _resolve_installed_lib {
    my ($self) = @_;
    my $paths;
    my @candidates;

    $paths = $self->_paths();
    @candidates = (
        File::Spec->catfile($paths->{prefix_dir}, 'lib', 'libsixel.so.1'),
        File::Spec->catfile($paths->{prefix_dir}, 'lib', 'libsixel.so'),
        File::Spec->catfile($paths->{prefix_dir}, 'lib', 'libsixel.dylib'),
        File::Spec->catfile($paths->{prefix_dir}, 'bin', 'libsixel.dll'),
    );

    for my $candidate (@candidates) {
        return $candidate if -f $candidate;
    }

    die "bundled libsixel shared library was not found\n";
}

sub _build_bundled_libsixel {
    my ($self) = @_;
    my $paths;
    my $configure_script;
    my $marker;
    my $installed_lib;

    $paths = $self->_paths();
    $configure_script = File::Spec->catfile($paths->{repo_root}, 'configure');
    $marker = $self->_configured_marker();

    make_path($paths->{build_dir});
    make_path($paths->{prefix_dir});

    if (!-f $marker) {
        $self->_run_or_die(
            'sh',
            $configure_script,
            '--disable-static',
            '--enable-shared',
            '--disable-python',
            '--disable-ruby',
            '--disable-tests',
            "--prefix=$paths->{prefix_dir}",
        );
        open my $fh, '>', $marker or die "cannot create $marker: $!";
        print {$fh} "configured\n";
        close $fh;
    }

    $self->_run_or_die('make', '-j4');
    $self->_run_or_die('make', 'install');

    $installed_lib = $self->_resolve_installed_lib();
    return $installed_lib;
}

sub _stage_shared_library {
    my ($self, $installed_lib) = @_;
    my $paths;
    my $staged_lib_name;
    my $staged_lib;

    $paths = $self->_paths();
    make_path($paths->{stage_lib_dir});

    $staged_lib_name = basename($installed_lib);
    $staged_lib = File::Spec->catfile($paths->{stage_lib_dir}, $staged_lib_name);
    copy($installed_lib, $staged_lib)
        or die "copy failed: $installed_lib -> $staged_lib: $!";
}

sub ACTION_build {
    my ($self) = @_;
    my $paths;
    my $cwd;
    my $installed_lib;

    $self->SUPER::ACTION_build();

    $paths = $self->_paths();
    make_path($paths->{build_dir});
    $cwd = abs_path('.');
    chdir $paths->{build_dir} or die "cannot chdir to $paths->{build_dir}: $!";
    $installed_lib = $self->_build_bundled_libsixel();
    chdir $cwd or die "cannot chdir to $cwd: $!";

    $self->_stage_shared_library($installed_lib);
}

1;
