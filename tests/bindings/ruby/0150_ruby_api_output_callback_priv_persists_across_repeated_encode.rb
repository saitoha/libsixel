#!/usr/bin/env ruby
# frozen_string_literal: true

puts '1..1'

begin
  require 'libsixel'

  priv = Object.new
  called = 0
  saw_same_priv = true
  output = Output.new(
    write_proc: proc do |_chunk, callback_priv|
      called += 1
      saw_same_priv = saw_same_priv && callback_priv.equal?(priv)
    end,
    priv: priv
  )
  dither = Libsixel::API.sixel_dither_get(Libsixel::API::SIXEL_BUILTIN_XTERM256)
  pixels = [255, 0, 0].pack('C*')
  depth = Libsixel::API.sixel_helper_compute_depth(Libsixel::API::SIXEL_PIXELFORMAT_RGB888)
  first = Libsixel::API.sixel_encode(pixels, 1, 1, depth, dither, output.ptr)
  second = Libsixel::API.sixel_encode(pixels, 1, 1, depth, dither, output.ptr)

  if Libsixel::API.failed?(first)
    puts 'not ok 1 - first output encode failed unexpectedly'
  elsif Libsixel::API.failed?(second)
    puts 'not ok 1 - second output encode failed unexpectedly'
  elsif called < 2
    puts 'not ok 1 - output callback was not invoked for both encodes'
  elsif !saw_same_priv
    puts 'not ok 1 - output callback priv was not preserved'
  else
    puts 'ok 1 - output callback priv persists across repeated encode'
  end
rescue StandardError => e
  puts 'not ok 1 - output callback priv persistence check failed'
  puts "# #{e.class}: #{e.message}"
end
