#!/usr/bin/env ruby
# frozen_string_literal: true

puts '1..1'

begin
  require 'libsixel'

  dither = Dither.new(ncolors: 16)
  dither.ref
  dither.unref

  puts 'ok 1 - dither new/ref/unref lifecycle APIs are callable'
rescue StandardError => e
  puts 'not ok 1 - dither lifecycle API check failed'
  puts "# #{e.class}: #{e.message}"
end
