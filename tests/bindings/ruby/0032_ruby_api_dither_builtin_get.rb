#!/usr/bin/env ruby
# frozen_string_literal: true

puts '1..1'

begin
  require 'libsixel'

  dither = Dither.get(Libsixel::API::SIXEL_BUILTIN_XTERM256)
  palette_colors = dither.num_palette_colors
  dither.unref

  if palette_colors > 0
    puts "ok 1 - built-in dither returned usable context (#{palette_colors} colors)"
  else
    puts 'not ok 1 - built-in dither returned no palette colors'
  end
rescue StandardError => e
  puts 'not ok 1 - dither builtin get check failed'
  puts "# #{e.class}: #{e.message}"
end
