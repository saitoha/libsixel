require "libsixel/version"
require "fiddle"
require "fiddle/import"

begin
  require_relative "libsixel/constants"
rescue LoadError
  # constants.rb is generated at build time; proceed without it in dev
end

require_relative "libsixel/api"
require_relative "libsixel/encoder"
require_relative "libsixel/decoder"
require_relative "libsixel/output"
require_relative "libsixel/dither"
require_relative "libsixel/helper"
require_relative "libsixel/frame"

module Libsixel
  def self.set_threads(value)
    auto_requested = false
    text = nil
    count = 0

    if value.is_a?(String)
      text = value.strip
      if text.casecmp('auto').zero?
        auto_requested = true
        count = 0
      else
        begin
          count = Integer(text, 10)
        rescue ArgumentError
          raise ArgumentError,
                "threads must be a positive integer or 'auto'"
        end
      end
    elsif value == :auto
      auto_requested = true
      count = 0
    else
      begin
        count = Integer(value)
      rescue ArgumentError, TypeError
        raise ArgumentError,
              "threads must be a positive integer or 'auto'"
      end
    end

    if auto_requested == false && count < 1
      raise ArgumentError,
            "threads must be a positive integer or 'auto'"
    end

    API.sixel_set_threads(count)
    nil
  end
end

# emacs Local Variables:
# emacs mode: ruby
# emacs tab-width: 2
# emacs indent-tabs-mode: nil
# emacs ruby-indent-level: 2
# emacs End:
# vim: set expandtab ts=2 sts=2 sw=2 :
# EOF
#
# Copyright (c) 2025 libsixel developers. See `AUTHORS`.
#
# Permission is hereby granted, free of charge, to any person obtaining a copy of
# this software and associated documentation files (the "Software"), to deal in
# the Software without restriction, including without limitation the rights to
# use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of
# the Software, and to permit persons to whom the Software is furnished to do so,
# subject to the following conditions:
#
# The above copyright notice and this permission notice shall be included in all
# copies or substantial portions of the Software.
#
# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
# IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS
# FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR
# COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER
# IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
# CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
#
