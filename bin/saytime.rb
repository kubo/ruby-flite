#! /usr/bin/env ruby
#
# saytime.rb - talking clock
#
# Usage:
#  # talking clock once
#  ruby saytime.rb
#
#  # talking clock forever
#  ruby saytime.rb --loop
#
#  # talking clock 5 times
#  ruby saytime.rb --loop 5
#
#  # talking clock after sleeping 5 seconds
#  ruby saytime.rb --interval 5
#
#  # talking clock with 'slt' and 'awb' voices
#  ruby saytime.rb --voices=slt,awb
#
require 'flite'
require 'optparse'

available_voices = Flite.list_builtin_voices
available_voices.delete('awb_time') # exclude awb_time

$cached_voices = {}

def saytime(voice_name)
  sec, min, hour = Time.now.to_a

  case hour
  when 0
    hour = 12
  when 13 .. 23
    hour -= 12
  end

  case min
  when 0
    min = " o'clock"
  else
    min = ':%02d' % min
  end

  case sec
  when 0
    sec = "exactly"
  when 1
    sec = "and #{sec} second"
  else
    sec = "and #{sec} seconds"
  end

  text = "The time is #{hour}#{min} #{sec}."

  puts "text: #{text}"
  puts "voice: #{voice_name}"

  voice = ($cached_voices[voice_name] ||= Flite::Voice.new(voice_name))
  voice.speech(text)
end

voices = []
loop_count = 1
interval = nil

OptionParser.new do |opts|
  opts.on('--voices NAMES', "voice names (default: #{available_voices.join(',')})") {|v| voices += v.split(',')}
  opts.on('--loop [COUNT]', Integer, 'loop count (default: 1)') {|v| loop_count = v}
  opts.on('--interval INTERVAL', Integer, 'sleep interval between loops (default: 0)') {|v| interval = v}
end.parse!

if voices.size == 0
  voices = available_voices
end

if loop_count && loop_count <= 0
  puts "invalid loop_count #{loop_count}. It must be positive number."
  exit(1)
end

srand()
saytime(voices.sample)
cnt = 1
while loop_count.nil? || (cnt += 1) <= loop_count
  sleep(interval) if interval
  saytime(voices.sample)
end
