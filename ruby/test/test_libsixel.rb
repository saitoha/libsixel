require 'minitest_helper'

class TestLibsixelVersion < Minitest::Test
  def test_that_it_has_a_version_number
    refute_nil ::Libsixel::VERSION
  end
end

class TestLibsixel < Minitest::Test
  def setup
    skip("libsixel shared library not found: #{$LIBSIXEL_LOAD_ERROR}") unless $LIBSIXEL_LOADED
    @encoder = Encoder.new
  end

  def test_throws_runtime_error_when_invalid_option_detected
    assert_raises(RuntimeError) { @encoder.setopt('X', '16') }
  end

  def test_encoder_accepts_supported_flags
    options = {
      'o' => '-',
      '7' => nil,
      '8' => nil,
      'R' => nil,
      'p' => 16,
      'm' => 'palette.map',
      'e' => nil,
      'I' => nil,
      'b' => 'xterm16',
      'd' => 'atkinson',
      'f' => 'norm',
      's' => 'center',
      'c' => '10x10+0+0',
      'w' => '80%',
      'h' => '120',
      'r' => 'nearest',
      'q' => 'high',
      'l' => 'force',
      't' => 'rgb',
      'E' => 'fast',
      'B' => '#112233',
      'k' => nil,
      'i' => nil,
      'u' => nil,
      'n' => 1,
      'C' => 2,
      'g' => nil,
      'v' => nil,
      'S' => nil,
      '@' => nil,
      'P' => nil,
      'O' => nil,
      'D' => nil
    }

    options.each do |flag, value|
      encoder = Encoder.new
      encoder.setopt(flag, value)
    end
  end
end
