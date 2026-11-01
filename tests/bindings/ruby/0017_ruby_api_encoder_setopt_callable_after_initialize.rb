#!/usr/bin/env ruby
# frozen_string_literal: true

require 'fileutils'

puts '1..1'
artifact_local_dir = ENV.fetch('ARTIFACT_LOCAL_DIR')
FileUtils.mkdir_p(artifact_local_dir)


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
