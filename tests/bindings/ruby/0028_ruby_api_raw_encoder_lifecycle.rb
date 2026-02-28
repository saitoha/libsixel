#!/usr/bin/env ruby
# frozen_string_literal: true

require 'tmpdir'

puts '1..1'

begin
  require 'libsixel'

  source = File.expand_path('tests/data/inputs/snake_64.png', ENV.fetch('TOP_SRCDIR', Dir.pwd))

  Dir.mktmpdir('libsixel-ruby-0028') do |tmpdir|
    output = File.join(tmpdir, 'raw_encoder.six')

    out = Libsixel::API::Util.make_outptr
    status = Libsixel::API.sixel_encoder_new(out, 0)
    raise RuntimeError, "sixel_encoder_new failed: #{status}" if Libsixel::API.failed?(status)

    encoder = Libsixel::API::Util.read_outptr(out)
    begin
      status = Libsixel::API.sixel_encoder_setopt(
        encoder,
        Libsixel::API::SIXEL_OPTFLAG_OUTPUT.getbyte(0),
        output
      )
      raise RuntimeError, "sixel_encoder_setopt failed: #{status}" if Libsixel::API.failed?(status)

      status = Libsixel::API.sixel_encoder_encode(encoder, source)
      raise RuntimeError, "sixel_encoder_encode failed: #{status}" if Libsixel::API.failed?(status)
    ensure
      Libsixel::API.sixel_encoder_unref(encoder) if encoder && encoder.to_i != 0
    end

    if File.size(output).positive?
      puts 'ok 1 - raw encoder APIs create/configure/encode/release successfully'
    else
      puts 'not ok 1 - raw encoder output missing or empty'
    end
  end
rescue StandardError => e
  puts 'not ok 1 - raw encoder lifecycle check failed'
  puts "# #{e.class}: #{e.message}"
end
