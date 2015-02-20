/* -*- c-file-style: "ruby"; indent-tabs-mode: nil -*-
 *
 * ruby-flite  - a small speech synthesis module
 * https://github.com/kubo/ruby-flite
 *
 * Copyright (C) 2015 Kubo Takehiro <kubo@jiubao.org>
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 *    1. Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *
 *    2. Redistributions in binary form must reproduce the above
 *       copyright notice, this list of conditions and the following
 *       disclaimer in the documentation and/or other materials provided
 *       with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHORS ''AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL <COPYRIGHT HOLDER> OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR
 * BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE
 * OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN
 * IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * The views and conclusions contained in the software and documentation
 * are those of the authors and should not be interpreted as representing
 * official policies, either expressed or implied, of the authors.
 */

#include <ruby.h>
#include "rbflite.h"

#ifndef STRINGIZE
#define STRINGIZE(expr) STRINGIZE0(expr)
#ifndef STRINGIZE0
#define STRINGIZE0(expr) #expr
#endif
#endif

#ifdef HAVE_LAME_LAME_H
#include <lame/lame.h>
#define HAVE_MP3LAME 1
#endif

#ifdef HAVE_LAME_H
#include <lame.h>
#define HAVE_MP3LAME 1
#endif

/* flite.dll */
typedef cst_val *(*audio_streaming_info_val_t)(const cst_audio_streaming_info *);
typedef void (*delete_voice_t)(cst_voice *);
typedef int (*flite_add_lang_t)(const char *, void (*)(cst_voice *), cst_lexicon *(*)());
typedef int (*flite_feat_remove_t)(cst_features *, const char *);
typedef void (*flite_feat_set_t)(cst_features *, const char *, const cst_val *);
typedef const char *(*flite_get_param_string_t)(const cst_features *, const char *, const char *);
typedef float (*flite_text_to_speech_t)(const char *, cst_voice *, const char *);
typedef cst_voice *(*flite_voice_load_t)(const char *);
typedef cst_audio_streaming_info *(*new_audio_streaming_info_t)();

/* flite_usenglish.dll */
typedef void (*usenglish_init_t)(cst_voice *);

/* flite_cmulex.dll */
typedef cst_lexicon *(*cmulex_init_t)(void);

/* flite_cmu_grapheme_lang.dll */
typedef void (*cmu_grapheme_lang_init_t)(cst_voice *);

/* flite_cmu_grapheme_lex.dll */
typedef cst_lexicon *(*cmu_grapheme_lex_init_t)(void);

/* flite_cmu_indic_lang.dll */
typedef void (*cmu_indic_lang_init_t)(cst_voice *);

/* flite_cmu_indic_lex.dll */
typedef cst_lexicon *(*cmu_indic_lex_init_t)(void);

#ifdef HAVE_MP3LAME
/* libmp3lame-0.dll */
typedef int (*lame_close_t)(lame_global_flags *);
typedef int (*lame_encode_buffer_t)(lame_global_flags *, const short int *, const short int *, const int , unsigned char *, const int);
typedef int (*lame_encode_flush_t)(lame_global_flags *, unsigned char *, int);
typedef lame_global_flags *(*lame_init_t)(void);
typedef int (*lame_init_params_t)(lame_global_flags *);
typedef int (*lame_set_bWriteVbrTag_t)(lame_global_flags *, int);
typedef int (*lame_set_brate_t)(lame_global_flags *, int);
typedef int (*lame_set_in_samplerate_t)(lame_global_flags *, int);
typedef int (*lame_set_mode_t)(lame_global_flags *, MPEG_mode);
typedef int (*lame_set_num_channels_t)(lame_global_flags *, int);
typedef int (*lame_set_num_samples_t)(lame_global_flags *, unsigned long);
typedef int (*lame_set_scale_t)(lame_global_flags *, float);
typedef int (*lame_set_quality_t)(lame_global_flags *, int);
#endif

static HMODULE this_module;
static CRITICAL_SECTION loading_mutex;

#define CALL_FUNC(dll_name, func_name, args, rettype)   \
    static func_name##_t func; \
    rettype rv; \
    if (func == NULL) { \
        func = (func_name##_t)load_func(dll_name##_dll(), #dll_name, #func_name); \
    } \
    rv = func args; \
    return rv;

#define CALL_FUNC0(dll_name, func_name, args)   \
    static func_name##_t func; \
    if (func == NULL) { \
        func = (func_name##_t)load_func(dll_name##_dll(), #dll_name, #func_name); \
    } \
    func args; \

#define CALL_LAME_FUNC(func_name, args, rettype)   \
    static func_name##_t func; \
    rettype rv; \
    if (func == NULL) { \
        func = (func_name##_t)load_func(libmp3lame_dll(), "libmp3lame-0.dll", #func_name); \
    } \
    rv = func args; \
    return rv;

static HMODULE load_module(const char *dll_name)
{
    static char path[MAX_PATH];
    static char *fname = NULL;
    HMODULE hModule;

    EnterCriticalSection(&loading_mutex);
    if (fname == NULL) {
        if (GetModuleFileNameA(this_module, path, sizeof(path)) == 0) {
            LeaveCriticalSection(&loading_mutex);
            rb_raise(rb_eLoadError, "Failed to get path of flite.so");
        }
        fname = strrchr(path, '\\') + 1;
        if (fname == NULL) {
            LeaveCriticalSection(&loading_mutex);
            rb_raise(rb_eLoadError, "Failed to directory name of flite.so");
        }
        if (fname - path + strlen(dll_name) + 1 >= MAX_PATH) {
            fname = NULL;
            LeaveCriticalSection(&loading_mutex);
            rb_raise(rb_eLoadError, "flite.so is in a too deep directory.");
        }
    }
    strcpy(fname, dll_name);
    hModule = LoadLibraryA(path);
    if (hModule == NULL) {
        LeaveCriticalSection(&loading_mutex);
        rb_raise(rb_eLoadError, "Failed to load %s: %s", path, rb_w32_strerror(-1));
    }
    LeaveCriticalSection(&loading_mutex);
    return hModule;
}

static FARPROC load_func(HMODULE hModule, const char *dll_name, const char *func_name)
{
    FARPROC func = GetProcAddress(hModule, func_name);
    if (func == NULL) {
        rb_raise(rb_eLoadError, "Failed to load symbol %s in %s.dll: %s", func_name, dll_name, rb_w32_strerror(-1));
    }
    return func;
}

static HMODULE flite_dll(void)
{
    static HMODULE hModule = NULL;
    if (hModule == NULL) {
        hModule = load_module("flite.dll");
    }
    return hModule;
}

static HMODULE flite_usenglish_dll(void)
{
    static HMODULE hModule = NULL;
    if (hModule == NULL) {
        flite_dll();
        hModule = load_module("flite_usenglish.dll");
    }
    return hModule;
}

static HMODULE flite_cmulex_dll(void)
{
    static HMODULE hModule = NULL;
    if (hModule == NULL) {
        flite_dll();
        hModule = load_module("flite_cmulex.dll");
    }
    return hModule;
}

static HMODULE flite_cmu_grapheme_lang_dll(void)
{
    static HMODULE hModule = NULL;
    if (hModule == NULL) {
        flite_dll();
        hModule = load_module("flite_cmu_grapheme_lang.dll");
    }
    return hModule;
}

static HMODULE flite_cmu_grapheme_lex_dll(void)
{
    static HMODULE hModule = NULL;
    if (hModule == NULL) {
        flite_dll();
        hModule = load_module("flite_cmu_grapheme_lex.dll");
    }
    return hModule;
}

static HMODULE flite_cmu_indic_lang_dll(void)
{
    static HMODULE hModule = NULL;
    if (hModule == NULL) {
        flite_usenglish_dll();
        hModule = load_module("flite_cmu_indic_lang.dll");
    }
    return hModule;
}

static HMODULE flite_cmu_indic_lex_dll(void)
{
    static HMODULE hModule = NULL;
    if (hModule == NULL) {
        flite_cmu_indic_lang_dll();
        hModule = load_module("flite_cmu_indic_lex.dll");
    }
    return hModule;
}

cst_voice *rbfile_call_voice_register_func(rbflite_builtin_voice_t *v, const char *voxdir)
{
    HMODULE hModule;

    flite_usenglish_dll();
    flite_cmulex_dll();
    hModule = load_module(v->dll_name);
    v->cached = (cst_voice **)load_func(hModule, v->dll_name, v->var_name);
    v->register_ = (cst_voice *(*)(const char *))load_func(hModule, v->dll_name, v->func_name);
    return v->register_(voxdir);
}

/* flite.dll */
cst_val *audio_streaming_info_val(const cst_audio_streaming_info *v)
{
    CALL_FUNC(flite, audio_streaming_info_val, (v), cst_val *);
}

/* flite.dll */
void delete_voice(cst_voice *u)
{
    CALL_FUNC0(flite, delete_voice, (u));
}

/* flite.dll */
int flite_add_lang(const char *langname, void (*lang_init)(cst_voice *vox), cst_lexicon *(*lex_init)())
{
    CALL_FUNC(flite, flite_add_lang, (langname, lang_init, lex_init), int);
}

/* flite.dll */
int flite_feat_remove(cst_features *f, const char *name)
{
    CALL_FUNC(flite, flite_feat_remove, (f, name), int);
}

/* flite.dll */
void flite_feat_set(cst_features *f, const char *name, const cst_val *v)
{
    CALL_FUNC0(flite, flite_feat_set, (f, name, v));
}

/* flite.dll */
const char *flite_get_param_string(const cst_features *f, const char *name, const char *def)
{
    CALL_FUNC(flite, flite_get_param_string, (f, name, def), const char *);
}

/* flite.dll */
float flite_text_to_speech(const char *text, cst_voice *voice, const char *outtype)
{
    CALL_FUNC(flite, flite_text_to_speech, (text, voice, outtype), float);
}

/* flite.dll */
cst_voice *flite_voice_load(const char *voice_filename)
{
    CALL_FUNC(flite, flite_voice_load, (voice_filename), cst_voice *);
}

/* flite.dll */
cst_audio_streaming_info *new_audio_streaming_info()
{
    CALL_FUNC(flite, new_audio_streaming_info, (), cst_audio_streaming_info *);
}

/* flite_usenglish.dll */
void usenglish_init(cst_voice *v)
{
    CALL_FUNC0(flite_usenglish, usenglish_init, (v));
}

/* flite_cmulex.dll */
cst_lexicon *cmulex_init(void)
{
    CALL_FUNC(flite_cmulex, cmulex_init, (), cst_lexicon *);
}

/* flite_cmu_grapheme_lang.dll */
void cmu_grapheme_lang_init(cst_voice *v)
{
    CALL_FUNC0(flite_cmu_grapheme_lang, cmu_grapheme_lang_init, (v));
}

/* flite_cmu_grapheme_lex.dll */
cst_lexicon *cmu_grapheme_lex_init(void)
{
    CALL_FUNC(flite_cmu_grapheme_lex, cmu_grapheme_lex_init, (), cst_lexicon *);
}

/* flite_cmu_indic_lang.dll */
void cmu_indic_lang_init(cst_voice *v)
{
    CALL_FUNC0(flite_cmu_indic_lang, cmu_indic_lang_init, (v));
}

/* flite_cmu_indic_lex.dll */
cst_lexicon *cmu_indic_lex_init(void)
{
    CALL_FUNC(flite_cmu_indic_lex, cmu_indic_lex_init, (), cst_lexicon *);
}

#ifdef HAVE_MP3LAME
static HMODULE libmp3lame_dll(void)
{
    static HMODULE hModule = NULL;
    if (hModule == NULL) {
        hModule = load_module("libmp3lame-0.dll");
    }
    return hModule;
}

int lame_close(lame_global_flags *gf)
{
    CALL_LAME_FUNC(lame_close, (gf), int);
}

int lame_encode_buffer(lame_global_flags *gf, const short int *buffer_l, const short int *buffer_r, const int nsamples, unsigned char *mp3buf, const int mp3buf_size)
{
    CALL_LAME_FUNC(lame_encode_buffer, (gf, buffer_l, buffer_r, nsamples, mp3buf, mp3buf_size), int);
}

int lame_encode_flush(lame_global_flags *gf, unsigned char *mp3buf, int size)
{
    CALL_LAME_FUNC(lame_encode_flush, (gf, mp3buf, size), int);
}

lame_global_flags *lame_init(void)
{
    CALL_LAME_FUNC(lame_init, (), lame_global_flags *);
}

int lame_init_params(lame_global_flags *gf)
{
    CALL_LAME_FUNC(lame_init_params, (gf), int);
}

int lame_set_bWriteVbrTag(lame_global_flags *gf, int bWriteVbrTag)
{
    CALL_LAME_FUNC(lame_set_bWriteVbrTag, (gf, bWriteVbrTag), int);
}

int lame_set_brate(lame_global_flags *gf, int brate)
{
    CALL_LAME_FUNC(lame_set_brate, (gf, brate), int);
}

int lame_set_in_samplerate(lame_global_flags *gf, int in_samplerate)
{
    CALL_LAME_FUNC(lame_set_in_samplerate, (gf, in_samplerate), int);
}

int lame_set_mode(lame_global_flags *gf, MPEG_mode mode)
{
    CALL_LAME_FUNC(lame_set_mode, (gf, mode), int);
}

int lame_set_num_channels(lame_global_flags *gf, int num_channels)
{
    CALL_LAME_FUNC(lame_set_num_channels, (gf, num_channels), int);
}

int lame_set_num_samples(lame_global_flags *gf, unsigned long num_samples)
{
    CALL_LAME_FUNC(lame_set_num_samples, (gf, num_samples), int);
}

int lame_set_scale(lame_global_flags *gf, float scale)
{
    CALL_LAME_FUNC(lame_set_scale, (gf, scale), int);
}

int lame_set_quality(lame_global_flags *gf, int quality)
{
    CALL_LAME_FUNC(lame_set_quality, (gf, quality), int);
}
#endif

__declspec(dllexport)
BOOL WINAPI DllMain(HINSTANCE hinstDLL, DWORD fdwReason, LPVOID lpvReserved)
{
    switch (fdwReason) {
    case DLL_PROCESS_ATTACH:
        this_module = hinstDLL;
        InitializeCriticalSection(&loading_mutex);
        break;
    case DLL_PROCESS_DETACH:
        DeleteCriticalSection(&loading_mutex);
        break;
    }
    return TRUE;
}
