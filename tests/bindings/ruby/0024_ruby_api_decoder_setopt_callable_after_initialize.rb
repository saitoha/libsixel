#!/usr/bin/env ruby
# frozen_string_literal: true

require 'fileutils'

puts '1..1'
artifact_local_dir = ENV.fetch('ARTIFACT_LOCAL_DIR')
FileUtils.mkdir_p(artifact_local_dir)


begin
  require 'libsixel'

  png = File.join(ENV.fetch('ARTIFACT_LOCAL_DIR'), 'decoder_post_init_setopt.png')
  decoder = Decoder.new
  decoder.setopt('o', png)

  puts 'ok 1 - decoder setopt is callable after initialize'
rescue StandardError => e
  puts 'not ok 1 - decoder post-initialize setopt check failed'
  puts "# #{e.class}: #{e.message}"
end
