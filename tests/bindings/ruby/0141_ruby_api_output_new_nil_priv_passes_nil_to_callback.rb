#!/usr/bin/env ruby
# frozen_string_literal: true

puts '1..1'

begin
  require 'libsixel'

  saw_nil_priv = false
  output = Output.new(write_proc: lambda { |_chunk, priv|
    saw_nil_priv ||= priv.nil?
  })

  dither = Libsixel::API.sixel_dither_get(Libsixel::API::SIXEL_BUILTIN_XTERM256)
  pixels = [255, 0, 0].pack('C*')
  depth = Libsixel::API.sixel_helper_compute_depth(Libsixel::API::SIXEL_PIXELFORMAT_RGB888)
  status = Libsixel::API.sixel_encode(pixels, 1, 1, depth, dither, output.ptr)

  if Libsixel::API.failed?(status)
    puts 'not ok 1 - encode failed while checking nil priv callback behavior'
  elsif saw_nil_priv
    puts 'ok 1 - output callback receives nil priv when not specified'
  else
    puts 'not ok 1 - output callback did not observe nil priv'
  end
rescue StandardError => e
  puts 'not ok 1 - output nil priv callback check failed'
  puts "# #{e.class}: #{e.message}"
end
