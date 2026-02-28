#!/usr/bin/env ruby
# frozen_string_literal: true

puts '1..1'

begin
  require 'libsixel'

  dither = Dither.get(Libsixel::API::SIXEL_BUILTIN_XTERM256)
  count = dither.num_palette_colors
  dither.unref

  if count > 0
    puts "ok 1 - dither palette color count getter returned positive value (#{count})"
  else
    puts 'not ok 1 - dither palette color count is not positive'
  end
rescue StandardError => e
  puts 'not ok 1 - dither palette color count check failed'
  puts "# #{e.class}: #{e.message}"
end
