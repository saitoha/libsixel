#!/usr/bin/env ruby
# frozen_string_literal: true

puts '1..1'

begin
  require 'libsixel'

  chunks = []
  priv_obj = { marker: 'priv-marker' }
  callback_ok = true

  output = Output.new(
    write_proc: lambda { |data, priv|
      callback_ok &&= priv.equal?(priv_obj)
      chunks << data
      nil
    },
    priv: priv_obj
  )
  dither = Dither.get(Libsixel::API::SIXEL_BUILTIN_XTERM256)
  pixels = [
    255, 0, 0,
    0, 255, 0,
    0, 0, 255,
    255, 255, 255
  ].pack('C*')
  depth = Libsixel::Helper.compute_depth(Libsixel::API::SIXEL_PIXELFORMAT_RGB888)
  status = Libsixel::API.sixel_encode(pixels, 2, 2, depth, dither.ptr, output.ptr)
  dither.unref
  output.unref

  if status != 0
    puts "not ok 1 - sixel_encode failed with status #{status}"
  elsif !callback_ok
    puts 'not ok 1 - output callback priv was not preserved'
  elsif chunks.empty?
    puts 'not ok 1 - output callback was not invoked'
  else
    puts 'ok 1 - output callback receives priv object from output new'
  end
rescue StandardError => e
  puts 'not ok 1 - output callback priv passthrough check failed'
  puts "# #{e.class}: #{e.message}"
end
