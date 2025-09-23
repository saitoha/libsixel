#!/usr/bin/env ruby
#
# An example usage of libsixel Encoder object
#
# Hayaki Saito <saitoha@me.com>
#
# I declare this program is in Public Domain (CC0 - "No Rights Reserved"),
# This file is offered AS-IS, without any warranty.
#
$LOAD_PATH.unshift File.expand_path('../../ruby/lib', __dir__)
require 'libsixel'

abort("usage: #{$0} <image-file>") unless ARGV.size == 1
path = ARGV[0]

enc = Encoder.new
# Example options (see include/sixel.h for flags)
enc.setopt('p', 64)  # colors
enc.encode(path)

# emacs Local Variables:
# emacs mode: ruby
# emacs tab-width: 2
# emacs indent-tabs-mode: nil
# emacs ruby-indent-level: 2
# emacs End:
# vim: set expandtab ts=2 sts=2 sw=2 :
# EOF
