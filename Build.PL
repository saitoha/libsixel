use Module::Build;

my $builder = Module::Build->new (
  module_name    => 'Image::Sixel',
  license        => 'mit',
  dist_abstract  => 'libsixel binding for Perl',

  requires       => {
  },

  build_requires => {
    'Test::More' => 0,
  },

  script_files   => [
  ],

  extra_compiler_flags => scalar `pkg-config libsixel --cflags` . ' -Wall -Wextra',
  extra_linker_flags   => scalar `pkg-config libsixel --libs`,
  create_makefile_pl   => 'passthrough',
);

$builder->create_build_script();
