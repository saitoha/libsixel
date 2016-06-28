# coding: utf-8
lib = File.expand_path('../lib', __FILE__)
$LOAD_PATH.unshift(lib) unless $LOAD_PATH.include?(lib)
require 'libsixel/version'

Gem::Specification.new do |spec|
  spec.name          = "libsixel-ruby"
  spec.version       = Libsixel::VERSION
  spec.authors       = ["Hayaki Saito"]
  spec.email         = ["saitoha@me.com"]
  spec.extensions    = ["ext/libsixel/extconf.rb"]
  spec.summary       = %q{A ruby interface to libsixel}
  spec.description   = %q{libsixel is a lightweight, fast implementation of DEC SIXEL graphics codec}
  spec.homepage      = "http://saitoha.github.com/libsixel"
  spec.license       = "MIT"

  spec.files         = `git ls-files -z`.split("\x0")
  spec.executables   = spec.files.grep(%r{^bin/}) { |f| File.basename(f) }
  spec.test_files    = spec.files.grep(%r{^(test|spec|features)/})
  spec.require_paths = ["lib"]

  spec.add_development_dependency "bundler", "~> 1.7"
  spec.add_development_dependency "rake", "~> 10.0"
  spec.add_development_dependency "rake-compiler"
  spec.add_development_dependency "minitest"
end
