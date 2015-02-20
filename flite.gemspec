# -*- ruby -*-

lib = File.expand_path('../lib', __FILE__)
$LOAD_PATH.unshift(lib) unless $LOAD_PATH.include?(lib)
require 'flite/version'

if ARGV.include?("--") and ARGV[(ARGV.index("--") + 1)] == 'current'
  gem_platform = 'current'
else
  gem_platform = Gem::Platform::RUBY
end

Gem::Specification.new do |spec|
  spec.name          = "flite"
  spec.version       = Flite::VERSION
  spec.authors       = ["Kubo Takehiro"]
  spec.email         = ["kubo@jiubao.org"]
  spec.summary       = %q{a small speech synthesis library}
  spec.description   = <<EOS
Ruby-flite is a small speech synthesis library for ruby using
CMU flite[http://cmuflite.org].

CMU Flite (festival-lite) is a small, fast run-time synthesis engine
developed at CMU and primarily designed for small embedded machines
and/or large servers. Flite is designed as an alternative synthesis
engine to Festival for voices built using the FestVox suite of voice
building tools.
EOS
  spec.homepage      = "https://github.com/kubo/ruby-flite/"
  spec.license       = "2-clause BSD-style license"
  spec.platform      = gem_platform

  files = `git ls-files -z`.split("\x0")
  files.delete('build.bat')
  files.delete('.gitignore')
  if gem_platform == 'current'
    files += Dir.glob('lib/flite_*.so')
    files += Dir.glob('lib/flite*.dll')
    files << 'lib/libmp3lame-0.dll'
  else
    spec.extensions  = ["ext/flite/extconf.rb"]
  end
  spec.files         = files
  spec.executables   = spec.files.grep(%r{^bin/}) { |f| File.basename(f) }
  spec.test_files    = spec.files.grep(%r{^(test|spec|features)/})
  spec.require_paths = ["lib"]
  spec.required_ruby_version = '>= 2.0.0'

  spec.add_development_dependency "bundler", "~> 1.7"
  spec.add_development_dependency "rake", "~> 10.0"
  spec.add_development_dependency "rake-compiler", '~> 0'
end
