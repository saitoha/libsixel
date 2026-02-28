#!/usr/bin/env ruby
# frozen_string_literal: true

puts '1..1'

begin
  require 'libsixel'

  out = Libsixel::API::Util.make_outptr
  status = Libsixel::API.sixel_encoder_new(out, 0)
  raise RuntimeError, "sixel_encoder_new failed: #{status}" if Libsixel::API.failed?(status)

  encoder = Libsixel::API::Util.read_outptr(out)
  Libsixel::API.sixel_encoder_unref(encoder)
  Libsixel::API.sixel_encoder_unref(encoder)

  puts 'ok 1 - raw encoder unref is callable twice without raising'
rescue StandardError => e
  puts 'not ok 1 - raw encoder unref idempotent check failed'
  puts "# #{e.class}: #{e.message}"
end
