#!/usr/bin/env ruby
# frozen_string_literal: true

puts '1..1'

begin
  require 'libsixel'

  decoder = Decoder.new
  begin
    decoder.setopt(Libsixel::API::SIXEL_OPTFLAG_OUTPUT, 'dummy.png')
    puts 'ok 1 - decoder setopt accepts character option flag'
  rescue RuntimeError, TypeError, ArgumentError => e
    puts 'not ok 1 - decoder setopt rejected character option flag'
    puts "# #{e.class}: #{e.message}"
  end
rescue StandardError => e
  puts 'not ok 1 - decoder character-flag setopt check failed'
  puts "# #{e.class}: #{e.message}"
end
