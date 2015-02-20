#
# ruby-flite  -  a small speech synthesis library
#   https://github.com/kubo/ruby-flite
#
# Copyright (C) 2015 Kubo Takehiro <kubo@jiubao.org>
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions are met:
#
#    1. Redistributions of source code must retain the above copyright
#       notice, this list of conditions and the following disclaimer.
#
#    2. Redistributions in binary form must reproduce the above
#       copyright notice, this list of conditions and the following
#       disclaimer in the documentation and/or other materials provided
#       with the distribution.
#
# THIS SOFTWARE IS PROVIDED BY THE AUTHORS ''AS IS'' AND ANY EXPRESS OR
# IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
# WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
# DISCLAIMED. IN NO EVENT SHALL <COPYRIGHT HOLDER> OR CONTRIBUTORS BE
# LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
# CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
# SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR
# BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
# WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE
# OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN
# IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
#
# The views and conclusions contained in the software and documentation
# are those of the authors and should not be interpreted as representing
# official policies, either expressed or implied, of the authors.

require "flite/version"
RUBY_VERSION =~ /(\d+).(\d+)/
require "flite_#{$1}#{$2}0"

module Flite
  # @private
  @@default_voice = Flite::Voice.new

  # Returns the voice used by {String#speak} and {String#to_speech}.
  #
  # @return [Flite::Voice]
  def self.default_voice
    @@default_voice
  end

  # Set the voice used by {String#speak} and {String#to_speech}.
  # When <code>name</code> is a {Flite::Voice}, use it.
  # Otherwise, use a new voice created by <code>Flite::Voice.new(name)</code>.
  #
  # @param [Flite::Voice or String] name voice or voice name
  # @see Flite::Voice#initialize
  def self.default_voice=(name)
    if name.is_a? Flite::Voice
      @@default_voice = name
    else
      @@default_voice = Flite::Voice.new(name)
    end
  end

  if RUBY_PLATFORM =~ /mingw32|win32/
    self.sleep_time_after_speaking = 0.3
  end
end

class String
  # Speaks <code>self</code>
  #
  # @example
  #   "Hello Flite World!".speak
  def speak
    Flite.default_voice.speak(self)
  end

  # @overload to_speech(audio_type = :wave, opts = {})
  #
  #  Converts <code>self</code> to audio data.
  #
  #  @example
  #    # Save speech as wav
  #    File.binwrite('hello_flite_world.wav',
  #                  'Hello Flite World!'.to_speech())
  #
  #    # Save speech as mp3
  #    File.binwrite('hello_flite_world.mp3',
  #                  'Hello Flite World!'to_speech(:mp3))
  #
  #    # Save speech as mp3 whose bitrate is 128k.
  #    File.binwrite('hello_flite_world.mp3',
  #                  'Hello Flite World!'.to_speech(:mp3, :bitrate => 128))
  #
  #  @param [Symbol] audo_type :wave or :mp3 (when mp3 support is enabled)
  #  @param [Hash]   opts  audio encoder options
  #  @return [String] audio data
  #  @see Flite.supported_audio_types
  def to_speech(*args)
    Flite.default_voice.to_speech(self, *args)
  end
end
