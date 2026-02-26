#!/usr/bin/env ruby
# frozen_string_literal: true

puts '1..1'

begin
  require 'libsixel'

  # SIXEL_PIXELFORMAT_RGB888 is 0x03 in include/sixel.h.
  depth = Libsixel::Helper.compute_depth(0x03)
  if depth == 3
    puts 'ok 1 - helper compute_depth returns expected value for RGB888'
  else
    puts "not ok 1 - unexpected depth for RGB888: #{depth}"
  end
rescue StandardError => e
  puts 'not ok 1 - helper compute_depth check failed'
  puts "# #{e.class}: #{e.message}"
end
