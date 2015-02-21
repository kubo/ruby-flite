#! /usr/bin/env ruby
#
# speaking_web_server.rb - Web server replying synthesized speech
#
# Usage:
#
# 1. Start a Web server.
#
#    ruby speaking_web_server.rb
#
# 2. Access to the web server.
#
#    Open browser and access: http://HOSTNAME_OR_IP_ADDRESS:9080
#
# 3. Click 'Play' buttons.
#
require 'webrick'
require 'flite'

def start_server(html_content)
  srv = WEBrick::HTTPServer.new({:Port => 9080})

  srv.mount_proc('/') do |req, res|
    h = req.query
    if h['text']
      audio_type = if h['type'] == 'mp3'
                     :mp3
                   else
                     :wav
                   end
      res['Content-type'] = "audio/#{audio_type}"
      res['Content-Disposition'] = %Q{attachment; filename="audio.#{audio_type}"}

      voice = Flite::Voice.new(h['voice'] || 'kal')
      res.body = voice.to_speech(h['text'], audio_type)
    else
      res['Content-type'] = 'text/html'
      res.body = html_content
    end
  end
  trap("INT"){ srv.shutdown }
  srv.start
end

html_content = <<EOS
<html>
<head>
<title>Flite CGI</title>

<script>
audio_type = 'wav';

function check_audio_type() {
  var audio = new Audio;
  if (audio.canPlayType('audio/wav') == '') {
    if (#{Flite.supported_audio_types.include? :mp3} && audio.canPlayType('audio/mp3') != '') {
      audio_type = 'mp3';
    } else {
      var text = document.getElementById('text');
      text.disabled = true;
      enable_buttons(false);
      alert('Cannot play audio/wav in this browser.\\nUse Chrome, Firefox, Safari or Opera.');
    }
  }
}

function speak(voice) {
  var text = document.getElementById('text');
  if (text != '') {
    var status = document.getElementById('status');
    status.innerHTML = 'Playing'
    enable_buttons(false);
    var url = '/?voice=' + voice + '&type=' + audio_type + '&text=' + encodeURIComponent(text.value);
    var audio = new Audio(url);
    audio.onended = function() {
      status.innerHTML = 'Finished'
      enable_buttons(true);
    };
    audio.onabort = function() {
      status.innerHTML = 'Abort'
      enable_buttons(true);
    };
    audio.onerror = function() {
      status.innerHTML = 'Error'
      enable_buttons(true);
    };
    audio.play();
  }
}

function enable_buttons(bval) {
  var form = document.getElementById('speech_form');
  var elements = form.elements;
  for (var i = 0; i < elements.length; i++) {
    var element = elements[i];
    if (element.tagName == 'INPUT') {
      element.disabled = !bval;
    }
  }
}
</script>

</head>
<body onLoad="check_audio_type();">

<form id="speech_form">
  <textarea id="text" name="text" cols=80 rows=4>Hello Flite World!</textarea>
  <br />
  <input type="button" value="Play(voice: kal)" onClick="speak('kal');">
  <input type="button" value="Play(voice: kal16)" onClick="speak('kal16');">
  <input type="button" value="Play(voice: awb)" onClick="speak('awb');">
  <input type="button" value="Play(voice: rms)" onClick="speak('rms');">
  <input type="button" value="Play(voice: slt)" onClick="speak('slt');">
</form>
Status: <span id="status" />
</body>
</html>
EOS

start_server(html_content)
