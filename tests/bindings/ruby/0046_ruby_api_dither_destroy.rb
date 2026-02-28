#!/usr/bin/env ruby
# frozen_string_literal: true

puts '1..1'

begin
  require 'libsixel'

  dither_ptr = Libsixel::API.sixel_dither_create(16)
  if dither_ptr.nil? || dither_ptr.to_i == 0
    puts 'not ok 1 - dither create returned null pointer'
  else
    Libsixel::API.sixel_dither_destroy(dither_ptr)
    puts 'ok 1 - dither destroy API is callable'
  end
rescue StandardError => e
  puts 'not ok 1 - dither destroy API check failed'
  puts "# #{e.class}: #{e.message}"
end
