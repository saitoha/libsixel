require 'minitest_helper'

class TestLibsixel < MiniTest::Unit::TestCase

  def setup
    @encoder = Encoder.new
  end

  def test_that_it_has_a_version_number
    refute_nil ::Libsixel::VERSION
  end

  def test_throws_runtime_error_when_invalid_option_detected
    begin
      @encoder.setopt('X', '16')
      assert false
    rescue RuntimeError
      assert true
    end
  end
end
