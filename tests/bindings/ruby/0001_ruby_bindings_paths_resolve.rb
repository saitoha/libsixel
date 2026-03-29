#!/usr/bin/env ruby
# frozen_string_literal: true

puts '1..1'

begin
  require 'libsixel'

  libsixel_rb = $LOADED_FEATURES.find { |path| path.end_with?('/libsixel.rb') }
  raise RuntimeError, 'failed to resolve libsixel.rb path' if libsixel_rb.nil?

  libs_dir = File.expand_path('libsixel/_libs', File.dirname(libsixel_rb))
  raise RuntimeError, "bundled library directory is missing: #{libs_dir}" unless Dir.exist?(libs_dir)

  candidates = Dir.glob(File.join(libs_dir, '*.{so,dylib,dll}')) +
               Dir.glob(File.join(libs_dir, '*.so.*'))
  raise RuntimeError, 'bundled shared library is missing' if candidates.empty?

  puts 'ok 1 - packaged ruby binding paths are configured'
rescue StandardError => e
  puts 'not ok 1 - ruby binding path resolution check failed'
  puts "# #{e.class}: #{e.message}"
end
