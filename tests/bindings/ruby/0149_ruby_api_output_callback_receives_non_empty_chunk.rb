#!/usr/bin/env ruby
# frozen_string_literal: true

puts '1..1'

begin
  require 'libsixel'

  saw_string = false
  saw_non_empty = false
  output = Output.new(
    write_proc: proc do |chunk, _priv|
      saw_string = saw_string || chunk.is_a?(String)
      saw_non_empty = saw_non_empty || !chunk.empty?
    end
  )
  dither = Libsixel::API.sixel_dither_get(Libsixel::API::SIXEL_BUILTIN_XTERM256)
  pixels = [255, 0, 0].pack('C*')
  depth = Libsixel::API.sixel_helper_compute_depth(Libsixel::API::SIXEL_PIXELFORMAT_RGB888)
  status = Libsixel::API.sixel_encode(pixels, 1, 1, depth, dither, output.ptr)

  if Libsixel::API.failed?(status)
    puts 'not ok 1 - output encode failed unexpectedly'
  elsif !saw_string
    puts 'not ok 1 - output callback chunk was not String'
  elsif !saw_non_empty
    puts 'not ok 1 - output callback chunk was unexpectedly empty'
  else
    puts 'ok 1 - output callback receives non-empty String chunk'
  end
rescue StandardError => e
  puts 'not ok 1 - output callback chunk verification failed'
  puts "# #{e.class}: #{e.message}"
end
