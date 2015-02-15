# Ruby Flite

Ruby Flite is a small speech synthesis library for ruby using [CMU Flite](http://cmuflite.org).

CMU Flite (festival-lite) is a small, fast run-time synthesis engine developed
at CMU and primarily designed for small embedded machines and/or large
servers. Flite is designed as an alternative synthesis engine to [Festival](http://festvox.org/festival) for
voices built using the [FestVox](http://festvox.org/) suite of voice building tools. 

## Supported versions

* ruby 1.9.3 and uppper
* CMU Flite 1.4 and 2.0.

## Installation

Install [CMU Flite](http://cmuflite.org):

```shell
# On ubuntu
sudo apt-get install flite1-dev

# On redhat
yum install flite flite-devel

# On Windows
# You have no need to install CMU Flite if you use rubies distributed by rubyinstaller.org.
# Binary gems for the rubies include CMU Flite.

# Others
# You need to install it by yourself.
```

And then execute:

    $ gem install flite

Ruby Flite tries to link with **all voices and languages**.
If you want to reduce dependent libraries, execute the followings
instead of above command.

    $ gem install flite -- --with-voices=kal --with-langs=eng

## Simple Usage

```ruby
require 'flite'

# output to the PC speaker.
"Hello World!".speak

# convert to a WAVE file
"Hello World!".to_speech

# save as a WAVE file
File.binwrite("hello_world.wav", "Hello World!".to_speech)
end
```
## Advanced Usage

```ruby
require 'flite'

# array of builtin voice names.
Flite.list_builtin_voices

# create a voice. 'slt' is a voice name.
voice = Flite::Voice.new("slt")

# output to the PC speaker.
voice.speak("Hello World!")

# convert to a WAVE file
voice.to_speech("Hello World!")

# save as a WAVE file
File.binwrite("hello_world.wav", voice.to_speech("Hello World!"))
end

# Change the voice used for String#to_speech
Flite.default_voice = 'rms'
```

## Sample Applications

* [saytime.rb](https://github.com/kubo/ruby-flite/blob/master/bin/saytime.rb) - talking clock
* [speech_web_server.rb](https://github.com/kubo/ruby-flite/blob/master/bin/speech_web_server.rb) - Web server replying synthesized speech

## Restrictions

* Ruby process doesn't terminate while talking to the speaker.

* When an error occurs in CMU Flite, the error message is outputted to
  the standard error.

## NEWS

### 0.1.0

Almost methods were changed.

Added methods:

* File::Voice#speak  - talks to the PC speaker
* File::Voice#to_speech - converts to audio data
* String#speak  - talks to the PC speaker

Deleted method:

* File::Voice#speech - use File::Voice#speak or File::Voice#to_speech instead

Changed Method:

* String#to_speech - converts to audio data. Use String#speak to talk to the PC speaker

## License

* Ruby Flite itself is licensed under 2-clause BSD-style license.
* CMU Flite is licensed under BSD-like license.
  See http://www.festvox.org/flite/download.html

## Related Works

* [flite4r](http://www.rubydoc.info/gems/flite4r/) - Flite for Ruby (GPL)
* [FestivalTTS4r](https://github.com/spejman/festivaltts4r) - Festival Text-To-Speech for Ruby
* [saytime](http://acme.com/software/saytime/) - talking clock for SPARCstations
 
## Contributing

1. Fork it ( https://github.com/kubo/ruby-flite/fork )
2. Create your feature branch (`git checkout -b my-new-feature`)
3. Commit your changes (`git commit -am 'Add some feature'`)
4. Push to the branch (`git push origin my-new-feature`)
5. Create a new Pull Request
