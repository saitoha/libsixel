#!/usr/bin/env ruby
# frozen_string_literal: true

puts '1..1'

begin
  require 'libsixel'

  out = Libsixel::API::Util.make_outptr
  status = Libsixel::API.sixel_decoder_new(out, 0)
  raise RuntimeError, "sixel_decoder_new failed: #{status}" if Libsixel::API.failed?(status)

  decoder = Libsixel::API::Util.read_outptr(out)
  Libsixel::API.sixel_decoder_unref(decoder)
  Libsixel::API.sixel_decoder_unref(decoder)

  puts 'ok 1 - raw decoder unref is callable twice without raising'
rescue StandardError => e
  puts 'not ok 1 - raw decoder unref idempotent check failed'
  puts "# #{e.class}: #{e.message}"
end
