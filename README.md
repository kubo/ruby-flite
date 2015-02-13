# ruby-flite

Ruby-flite is a small speech synthesis library for ruby using [CMU Flite](http://cmuflite.org).

CMU Flite (festival-lite) is a small, fast run-time synthesis engine developed
at CMU and primarily designed for small embedded machines and/or large
servers. Flite is designed as an alternative synthesis engine to [Festival](http://festvox.org/festival) for
voices built using the [FestVox](http://festvox.org/) suite of voice building tools. 

## Supported versions

* ruby 2.0.0 and uppper
* CMU Flite 1.4 and 2.0.

## Installation

Add this line to your application's Gemfile:

```ruby
gem 'flite'
```

Install [CMU Flite](http://cmuflite.org):

```shell
# On ubuntu
sudo apt-get install flite1-dev

# On redhat
yum install flite flite-devel

# On Windows
# you have no need to install CMU Flite if you use the flite binary gem.
# CMU Flite is statically linked.
```

And then execute:

    $ bundle

Or install it yourself as:

    $ gem install flite

Ruby-flite tries to link with **all voices and languages**.
If you want to reduce dependent libraries, execute the followings
instead of above commands.

    $ bundle config --local build.flite --with-voices=kal --with-langs=eng
    $ bundle

Or install it yourself as:

    $ gem install flite -- --with-voices=kal --with-langs=eng

## Simple Usage

```ruby
require 'flite'

# output to the speeker.
"Hello World!".to_speech

# save as a WAVE file
"Hello World!".to_speech("hello_world.wave")

# write to an I/O object if it responds to 'write'.
File.open("hello_world.wave", "wb") do |f|
  "Hello World!".to_speech(f)
end
```
## Advanced Usage

```ruby
require 'flite'

# array of builtin voice names.
Flite.list_builtin_voices

# create a voice. 'slt' is a voice name.
voice = Flite::Voice.new("slt")

# output to the speeker.
voice.speech("Hello World!")

# save as a WAVE file
voice.speech("Hello World!", "hello_world.wave")

# write to an I/O object if it responds to 'write'.
File.open("hello_world.wave", "wb") do |f|
  voice.speech("Hello World!", f)
end

# Change the voice used for String#to_speech
Flite.default_voice = 'rms'
```

## Sample Application

* [saytime.rb](https://github.com/kubo/ruby-flite/blob/master/bin/saytime.rb)

## Restrictions

* `String#to_speech(io_object)` and `Flite::Voice#speech(text, io_object)`
  are not thread-safe. You need to create `Flite::Voice` objects for
  each threads and use `Flite::Voice#speech`.

* `String#to_speech("play")` and `Flite::Voice#speech(text, "play")`
  don't save wave data to the specified file `play`. They output speech
  data to the speaker instead.

* `String#to_speech("stream")`, `String#to_speech("none")`,
  `Flite::Voice#speech(text, "stream")` and `Flite::Voice#speech(text, "none")`
  don't save wave data to the specified file `stream` or `none`. They
  synthesize speech and discard the result.

* When an error occurs in CMU Flite, the error message is outputted to
  the standard error.

## License

* Ruby-flite itself is licensed under 2-clause BSD-style license.
* CMU Flite is licensed under BSD-like license.
  See http://www.festvox.org/flite/download.html

## Contributing

1. Fork it ( https://github.com/kubo/ruby-flite/fork )
2. Create your feature branch (`git checkout -b my-new-feature`)
3. Commit your changes (`git commit -am 'Add some feature'`)
4. Push to the branch (`git push origin my-new-feature`)
5. Create a new Pull Request
