# Libsixel

The libsixel gem provides Ruby language bindings for libsixel(https://github.com/saitoha/libsixel).

## Requirements

- libsixel(>=1.5.0)

## Installation

```ruby
gem 'libsixel-ruby'
```

And then execute:

    $ bundle

Or install it yourself as:

    $ gem install libsixel-ruby

## Usage

```ruby
require 'libsixel'
encoder = Encoder.new
encoder.setopt 'p', 16
encoder.setopt 'w', '200'
encoder.encode 'images/egret.jpg'
```

## Contributing

1. Fork it ( https://github.com/[my-github-username]/libsixel-ruby/fork )
2. Create your feature branch (`git checkout -b my-new-feature`)
3. Commit your changes (`git commit -am 'Add some feature'`)
4. Push to the branch (`git push origin my-new-feature`)
5. Create a new Pull Request
