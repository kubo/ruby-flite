require "mkmf"

dir_config('flite')

libs_old = $libs

unless have_library('flite', 'flite_init')
  saved_libs = $libs
  puts "checkign for audio libraries depended by flite ..."
  unless [['asound'], ['winmm'], ['pulse-simple', 'pulse']].any? do |libs|
      $libs = saved_libs
      libs.all? { |lib| have_library(lib) } && have_library('flite', 'flite_init')
    end
    raise "Failed to find flite libraries."
  end
end

have_func('flite_voice_load')
have_func('flite_add_lang')
have_struct_member('cst_audio_streaming_info', 'utt', 'flite/cst_audio.h')

langs = with_config('langs', 'eng,indic,grapheme')

langs.split(',').each do |lang|
  lib = if lang == 'eng'
          ['flite_usenglish', 'usenglish_init',
           'flite_cmulex', 'cmu_lex_init']
        else
          ["flite_cmu_#{lang}_lang", "cmu_#{lang}_lang_init",
           "flite_cmu_#{lang}_lex", "cmu_#{lang}_lex_init"]
        end
  if have_library(lib[0], lib[1]) and have_library(lib[2], lib[3])
    $defs << "-DHAVE_LANG_#{lang.upcase}"
  end
end

builtin_voices = {
  'kal'      => ['cmu_us_kal',   'cmu_us_kal_diphone'],
  'awb_time' => ['cmu_time_awb', 'cmu_time_awb_ldom'],
  'kal16'    => ['cmu_us_kal16', 'cmu_us_kal16_diphone'],
  'awb'      => ['cmu_us_awb',   'cmu_us_awb_cg'],
  'rms'      => ['cmu_us_rms',   'cmu_us_rms_cg'],
  'slt'      => ['cmu_us_slt',   'cmu_us_slt_cg'],
}

voices = with_config('voices', 'kal,awb_time,kal16,awb,rms,slt')

voices = voices.split(',').inject([]) do |memo, name|
  v = builtin_voices[name]
  if v
    puts "checking for voice #{name}... "
    if have_library("flite_#{v[0]}", "register_#{v[0]}")
      memo << [name, *v]
    end
  else
    puts "warning: #{name} is not a builtin voice."
  end
  memo
end

File.open('rbflite_builtin_voice_list.c', 'w') do |f|
  f.write <<EOS
/*
 * This file is automatically generated by extconf.rb.
 * Don't edit this.
 */
#include "rbflite.h"

EOS
  voices.each do |v|
    f.puts "cst_voice *register_#{v[1]}(const char *voxdir);"
  end
  f.puts ""
  voices.each do |v|
    f.puts "extern cst_voice *#{v[2]};"
  end
  f.write <<EOS

#undef ENTRY
#ifdef RBFLITE_WIN32_BINARY_GEM
static cst_voice *dummy;
#define ENTRY(name, dll_name, func_name, var_name) {#name, func_name, &dummy, #dll_name, #func_name, #var_name}
#else
#define ENTRY(name, dll_name, func_name, var_name) {#name, func_name, &var_name}
const
#endif

rbflite_builtin_voice_t rbflite_builtin_voice_list[] = {
EOS
  voices.each do |v|
    f.puts("    ENTRY(#{v[0]}, flite_#{v[1]}.dll, register_#{v[1]}, #{v[2]}),")
  end
  f.write <<EOS
    {NULL, },
};

#ifdef RBFLITE_WIN32_BINARY_GEM
EOS
  voices.each_with_index do |v, idx|
    f.write <<EOS
cst_voice *register_#{v[1]}(const char *voxdir)
{
    return rbfile_call_voice_register_func(&rbflite_builtin_voice_list[#{idx}], voxdir);
}
EOS
  end
  f.write <<EOS
#endif
EOS
end

if have_library('mp3lame')
  have_header('lame.h') || have_header('lame/lame.h')
end

RUBY_VERSION =~ /(\d+).(\d+)/
$defs << "-DInit_flite=Init_flite_#{$1}#{$2}0"

$objs = ['rbflite.o', 'rbflite_builtin_voice_list.o']

if with_config('win32-binary-gem')
  $libs = libs_old
  $defs << "-DRBFLITE_WIN32_BINARY_GEM"
  $objs << 'win32_binary_gem.o'
end

create_makefile("flite_#{$1}#{$2}0")
