# Ruby Flite

Ruby Flite is a small speech synthesis library for ruby using [CMU Flite](http://cmuflite.org).

CMU Flite (festival-lite) is a small, fast run-time synthesis engine developed
at CMU and primarily designed for small embedded machines and/or large
servers. Flite is designed as an alternative synthesis engine to [Festival](http://festvox.org/festival) for
voices built using the [FestVox](http://festvox.org/) suite of voice building tools. 

## Supported versions

* ruby 2.0.0 and uppper
* CMU Flite 1.4 and 2.0.

## Installation

Install [CMU Flite](http://cmuflite.org) and (optionally) [LAME](http://lame.sourceforge.net/).

```shell
# On ubuntu
sudo apt-get install flite1-dev
sudo apt-get install libmp3lame-dev # for mp3 support (optionally)

# On redhat
yum install flite-devel
yum install libmp3lame-devel # for mp3 support (optionally)

# On Windows
# You have no need to install CMU Flite and LAME if you use rubies distributed by
# rubyinstaller.org. Binary gems for the rubies include them.

# Others
# You need to install them by yourself.
```

And then execute:

    $ gem install flite

Ruby Flite tries to link with **all voices and languages**.
If you want to reduce dependent libraries, execute the followings
instead of above command.

    $ gem install flite -- --with-voices=kal --with-langs=eng

## Examples

```ruby
require 'flite'

# Speak "Hello World!"
"Hello World!".speak

# Create a wav data and save as "hello_world.wav"
File.binwrite("hello_world.wav", "Hello World!".to_speech)

# Create a mp3 data and save as "hello_world.mp3"
# This works if mp3 support is enabled.
File.binwrite("hello_world.mp3", "Hello World!".to_speech(:mp3))

# Change the voice used for String#speak and String#to_speech
Flite.default_voice = 'rms'

# Speak again
"Hello World!".speak
```

See:

* http://www.rubydoc.info/gems/flite/Flite/Voice
* http://www.rubydoc.info/gems/flite/String

## Sample Applications

### [saytime](https://github.com/kubo/ruby-flite/blob/master/bin/saytime) - talking clock

This is inspired by [saytime - talking clock for SPARCstations](http://acme.com/software/saytime/).

Example:

> Talk the current time once:
>
> ```shell
> saytime
> ```
>
> Talk the current time forever:
>
> ```shell
> saytime --loop
> ```
>
> Talk the current time 5 times
>
> ```shell
> saytime --loop 5
> ```
>
> Talk the current time 5 times with 10 second intervals
>
> ```shell
> saytime --loop 5 --interval 10
> ```

### [speaking-web-server](https://github.com/kubo/ruby-flite/blob/master/bin/speaking-web-server) - Web server replying synthesized speech

Usage:

> Start a web server:
> 
> ```shell
> speaking-web-server
> ```
>
> Open a browser and access:
>
>     http://HOSTNAME_OR_IP_ADDRESS:9080
>     (Change HOSTNAME_OR_IP_ADDRESS.)
>
> Click 'Play' buttons.

## Restrictions

* Ruby process doesn't terminate while talking.

* When an error occurs in CMU Flite, the error message is outputted to
  the standard error.

* When a fatal error occurs in CMU Flite, the error message is outputted to
  the standard error and the process is terminated. (CMU Flite calls `exit(-1)`...)

## NEWS

### 0.1.0

Almost methods were changed. Especially {String#to_speech} returns WAV
audio data instead of speaking. Use {String#speak} to speak a text.

## License

* Ruby Flite is licensed under 2-clause BSD-style license.
* CMU Flite bundled in Windows binary gems is licensed under BSD-like license.
  See http://www.festvox.org/flite/download.html
* LAME bundled in Windows binary gems is licensed under LGPL.

## Related Works

* [flite4r](http://www.rubydoc.info/gems/flite4r/) - Flite for Ruby (GPL)
* [FestivalTTS4r](https://github.com/spejman/festivaltts4r) - Festival Text-To-Speech for Ruby
 
## Contributing

1. Fork it ( https://github.com/kubo/ruby-flite/fork )
2. Create your feature branch (`git checkout -b my-new-feature`)
3. Commit your changes (`git commit -am 'Add some feature'`)
4. Push to the branch (`git push origin my-new-feature`)
5. Create a new Pull Request
