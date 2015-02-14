#! /usr/bin/env ruby
#
# speech_web_server.rb - Web server replying synthesized speech
#
# Usage:
#
# 1. Start a Web server.
#
#    ruby speech_web_server.rb
#
# 2. Access to the web server.
#
#    Open browser and access: http://HOSTNAME:9080
#
# 3. Click 'Play' buttons.
#
require 'webrick'
require 'flite'
require 'stringio'

html_content = DATA.read

srv = WEBrick::HTTPServer.new({:Port => 9080})

srv.mount_proc('/') do |req, res|
  h = req.query
  if h['text']
    res['Content-type'] = 'audio/wav'
    res['Content-Disposition'] = 'attachment; filename="audio.wav"'

    voice = Flite::Voice.new(h['voice'] || 'kal')
    io = StringIO.new
    voice.speech(h['text'], io)
    io.rewind
    res.body = io.read
  else
    res['Content-type'] = 'text/html'
    res.body = html_content
  end
end

trap("INT"){ srv.shutdown }
srv.start

__END__
<html>
<head>
<title>Flite CGI</title>

<script>
function check_audio_type() {
  var audio = new Audio;
  if (audio.canPlayType('audio/wav') == '') {
    var text = document.getElementById('text');
    text.disabled = true;
    enable_buttons(false);
    alert('Cannot play audio/wav in this browser.\nUse Chrome, Firefox, Safari or Opera.');
  }
}

function speech(voice) {
  var text = document.getElementById('text');
  if (text != '') {
    enable_buttons(false);
    var url = '/?text=' + encodeURIComponent(text.value) + '&voice=' + encodeURIComponent(voice);
    var audio = new Audio(url);
    audio.onended = function() {
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
  <input type="button" value="Play(voice: kal)" onClick="speech('kal');">
  <input type="button" value="Play(voice: kal16)" onClick="speech('kal16');">
  <input type="button" value="Play(voice: awb)" onClick="speech('awb');">
  <input type="button" value="Play(voice: rms)" onClick="speech('rms');">
  <input type="button" value="Play(voice: slt)" onClick="speech('slt');">
</form>

</body>
</html>
