#!/usr/bin/env ruby
# frozen_string_literal: true

puts '1..1'

begin
  require 'libsixel'

  out = File.join(ENV.fetch('ARTIFACT_LOCAL_DIR'), 'encoder_post_init_setopt.six')
  encoder = Encoder.new
  encoder.setopt(Libsixel::API::SIXEL_OPTFLAG_OUTPUT, out)

  puts 'ok 1 - encoder setopt is callable after initialize'
rescue StandardError => e
  puts 'not ok 1 - encoder post-initialize setopt check failed'
  puts "# #{e.class}: #{e.message}"
end
