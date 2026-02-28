#!/usr/bin/env ruby
# frozen_string_literal: true

require 'pathname'
require 'tmpdir'

puts '1..1'

begin
  require 'libsixel'

  source = Pathname.new(File.expand_path('tests/data/inputs/snake_64.png', ENV.fetch('TOP_SRCDIR', Dir.pwd)))

  Dir.mktmpdir('libsixel-ruby-0093') do |tmpdir|
    output = Pathname.new(File.join(tmpdir, 'encode_pathlike.six'))

    encoder = Encoder.new
    encoder.setopt('o', output.to_s)
    encoder.encode(source)

    payload = File.binread(output)
    if payload.empty?
      puts 'not ok 1 - encoder output missing for path-like input'
    elsif payload.start_with?("\eP") && payload.end_with?("\e\\")
      puts 'ok 1 - encoder accepts path-like input'
    else
      puts 'not ok 1 - encoder output is not a valid sixel envelope'
    end
  end
rescue StandardError => e
  puts 'not ok 1 - encoder path-like input acceptance check failed'
  puts "# #{e.class}: #{e.message}"
end
