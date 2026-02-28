#!/usr/bin/env ruby
# frozen_string_literal: true

require 'tmpdir'

puts '1..1'

begin
  require 'libsixel'

  source = File.expand_path('tests/data/inputs/snake_64.png', ENV.fetch('TOP_SRCDIR', Dir.pwd))

  Dir.mktmpdir('libsixel-ruby-0029') do |tmpdir|
    sixel_path = File.join(tmpdir, 'raw_decoder_input.six')
    png_path = File.join(tmpdir, 'raw_decoder_output.png')

    enc_out = Libsixel::API::Util.make_outptr
    status = Libsixel::API.sixel_encoder_new(enc_out, 0)
    raise RuntimeError, "sixel_encoder_new failed: #{status}" if Libsixel::API.failed?(status)
    encoder = Libsixel::API::Util.read_outptr(enc_out)

    begin
      status = Libsixel::API.sixel_encoder_setopt(
        encoder,
        Libsixel::API::SIXEL_OPTFLAG_OUTPUT.getbyte(0),
        sixel_path
      )
      raise RuntimeError, "sixel_encoder_setopt failed: #{status}" if Libsixel::API.failed?(status)

      status = Libsixel::API.sixel_encoder_encode(encoder, source)
      raise RuntimeError, "sixel_encoder_encode failed: #{status}" if Libsixel::API.failed?(status)
    ensure
      Libsixel::API.sixel_encoder_unref(encoder) if encoder && encoder.to_i != 0
    end

    dec_out = Libsixel::API::Util.make_outptr
    status = Libsixel::API.sixel_decoder_new(dec_out, 0)
    raise RuntimeError, "sixel_decoder_new failed: #{status}" if Libsixel::API.failed?(status)
    decoder = Libsixel::API::Util.read_outptr(dec_out)

    begin
      status = Libsixel::API.sixel_decoder_setopt(
        decoder,
        Libsixel::API::SIXEL_OPTFLAG_INPUT.getbyte(0),
        sixel_path
      )
      raise RuntimeError, "sixel_decoder_setopt(input) failed: #{status}" if Libsixel::API.failed?(status)

      status = Libsixel::API.sixel_decoder_setopt(
        decoder,
        Libsixel::API::SIXEL_OPTFLAG_OUTPUT.getbyte(0),
        png_path
      )
      raise RuntimeError, "sixel_decoder_setopt(output) failed: #{status}" if Libsixel::API.failed?(status)

      status = Libsixel::API.sixel_decoder_decode(decoder)
      raise RuntimeError, "sixel_decoder_decode failed: #{status}" if Libsixel::API.failed?(status)
    ensure
      Libsixel::API.sixel_decoder_unref(decoder) if decoder && decoder.to_i != 0
    end

    if File.size(png_path).positive?
      puts 'ok 1 - raw decoder APIs create/configure/decode/release successfully'
    else
      puts 'not ok 1 - raw decoder output missing or empty'
    end
  end
rescue StandardError => e
  puts 'not ok 1 - raw decoder lifecycle check failed'
  puts "# #{e.class}: #{e.message}"
end
