#!/usr/bin/env ruby
# frozen_string_literal: true

puts '1..1'

begin
  require 'libsixel'

  src = File.join(ENV.fetch('TOP_SRCDIR'), 'README.md')
  encoder = Encoder.new

  begin
    encoder.setopt(Libsixel::API::SIXEL_OPTFLAG_LOADERS, "builtin!")
    encoder.encode(src)
    puts 'not ok 1 - encoder accepted unsupported format input'
  rescue RuntimeError
    puts 'ok 1 - encoder rejects unsupported format input'
  end
rescue StandardError => e
  puts 'not ok 1 - unsupported format rejection check failed'
  puts "# #{e.class}: #{e.message}"
end
