#!/usr/bin/env ruby
# frozen_string_literal: true

puts '1..1'

begin
  require 'libsixel'

  failing_output = Output.new(write_proc: proc { |_chunk, _priv| raise 'first callback failure' })
  dither = Libsixel::API.sixel_dither_get(Libsixel::API::SIXEL_BUILTIN_XTERM256)
  pixels = [255, 0, 0].pack('C*')
  depth = Libsixel::API.sixel_helper_compute_depth(Libsixel::API::SIXEL_PIXELFORMAT_RGB888)

  begin
    Libsixel::API.sixel_encode(pixels, 1, 1, depth, dither, failing_output.ptr)
  rescue StandardError
    # expected
  end

  called = false
  succeeding_output = Output.new(write_proc: proc { |_chunk, _priv| called = true })
  status = Libsixel::API.sixel_encode(pixels, 1, 1, depth, dither, succeeding_output.ptr)

  if Libsixel::API.failed?(status)
    puts 'not ok 1 - second output encode failed after first callback error'
  elsif called
    puts 'ok 1 - callback error on one output does not break another output'
  else
    puts 'not ok 1 - second output callback was not invoked'
  end
rescue StandardError => e
  puts 'not ok 1 - output callback isolation check failed'
  puts "# #{e.class}: #{e.message}"
end
