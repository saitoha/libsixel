#!/usr/bin/env ruby
# frozen_string_literal: true

puts '1..1'

begin
  require 'libsixel'

  out = Libsixel::API::Util.make_outptr
  status = Libsixel::API.sixel_decoder_new(out, 0)
  raise RuntimeError, "sixel_decoder_new failed: #{status}" if Libsixel::API.failed?(status)

  decoder = Libsixel::API::Util.read_outptr(out)
  begin
    begin
      Libsixel::API.sixel_decoder_setopt(decoder, 'i', 'dummy.png')
      puts 'not ok 1 - raw decoder_setopt accepted string option flag input'
    rescue TypeError
      puts 'ok 1 - raw decoder_setopt rejects string option flag input'
    end
  ensure
    Libsixel::API.sixel_decoder_unref(decoder) if decoder && decoder.to_i != 0
  end
rescue StandardError => e
  puts 'not ok 1 - raw decoder_setopt string option flag rejection check failed'
  puts "# #{e.class}: #{e.message}"
end
