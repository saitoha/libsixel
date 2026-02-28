#!/usr/bin/env ruby
# frozen_string_literal: true

puts '1..1'

begin
  require 'libsixel'

  out = Libsixel::API::Util.make_outptr
  status = Libsixel::API.sixel_encoder_new(out, 0)
  raise RuntimeError, "sixel_encoder_new failed: #{status}" if Libsixel::API.failed?(status)

  encoder = Libsixel::API::Util.read_outptr(out)
  begin
    begin
      Libsixel::API.sixel_encoder_setopt(encoder, 'p', '256')
      puts 'not ok 1 - raw encoder_setopt accepted string option flag input'
    rescue TypeError
      puts 'ok 1 - raw encoder_setopt rejects string option flag input'
    end
  ensure
    Libsixel::API.sixel_encoder_unref(encoder) if encoder && encoder.to_i != 0
  end
rescue StandardError => e
  puts 'not ok 1 - raw encoder_setopt string option flag rejection check failed'
  puts "# #{e.class}: #{e.message}"
end
