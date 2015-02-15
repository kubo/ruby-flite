/* -*- c-file-style: "ruby"; indent-tabs-mode: nil -*-
 *
 * ruby-flite  -  a small speech synthesis module
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
#include <ruby/thread.h>
#include <ruby/encoding.h>
#include "rbflite.h"
#include <flite/flite_version.h>

#ifndef MIN
#define MIN(a, b) ((a) < (b)) ? (a) : (b)
#endif
#ifndef MAX
#define MAX(a, b) ((a) > (b)) ? (a) : (b)
#endif

#ifdef WORDS_BIGENDIAN
#define TO_LE4(num)  SWAPINT(num)
#defien TO_LE2(num)  SWAPSHORT(num)
#else
#define TO_LE4(num)  (num)
#define TO_LE2(num)  (num)
#endif

#ifdef HAVE_CST_AUDIO_STREAMING_INFO_UTT
/* flite 2.0.0 */
typedef struct cst_audio_streaming_info_struct *asc_last_arg_t;
#define ASC_LAST_ARG_TO_USERDATA(last_arg) (last_arg)->userdata
#else
/* flite 1.4.0 */
typedef void *asc_last_arg_t;
#define ASC_LAST_ARG_TO_USERDATA(last_arg) (last_arg)
#endif

void usenglish_init(cst_voice *v);
cst_lexicon *cmulex_init(void);

void cmu_indic_lang_init(cst_voice *v);
cst_lexicon *cmu_indic_lex_init(void);

void cmu_grapheme_lang_init(cst_voice *v);
cst_lexicon *cmu_grapheme_lex_init(void);

typedef struct thread_queue_entry {
    struct thread_queue_entry *next;
    VALUE thread;
} thread_queue_entry_t;

typedef struct {
    thread_queue_entry_t *head;
    thread_queue_entry_t **tail;
} thread_queue_t;

typedef struct {
    cst_voice *voice;
    thread_queue_t queue;
} rbflite_voice_t;

#define MIN_BUFFER_LIST_SIZE (64 * 1024)
typedef struct buffer_list {
    struct buffer_list *next;
    size_t size;
    size_t used;
    char buf[1];
} buffer_list_t;

typedef struct {
    cst_voice *voice;
    const char *text;
    const char *outtype;
    buffer_list_t *buffer_list;
    buffer_list_t *buffer_list_last;
} voice_speech_arg_t;

static VALUE rb_mFlite;
static VALUE rb_cVoice;

static int rbflite_audio_write_cb(const cst_wave *w, int start, int size, int last, asc_last_arg_t last_arg);
static buffer_list_t *buffer_list_alloc(size_t size);

static void lock_thread(thread_queue_t *queue, thread_queue_entry_t *entry)
{
    /* enqueue the current thread to voice->queue. */
    entry->next = NULL;
    *queue->tail = entry;
    queue->tail = &entry->next;
    if (queue->head != entry) {
        /* stop the current thread if other threads run. */
        entry->thread = rb_thread_current();
        rb_thread_stop();
    }
}

static void unlock_thread(thread_queue_t *queue)
{
    /* dequeue the current thread from voice->queue. */
    queue->head = queue->head->next;
    if (queue->head == NULL) {
        queue->tail = &queue->head;
    } else {
        /* resume the top of blocked threads. */
        rb_thread_wakeup_alive(queue->head->thread);
    }
}

static int add_data(voice_speech_arg_t *arg, const void *data, size_t size)
{
    buffer_list_t *list;
    size_t rest;

    if (arg->buffer_list == NULL) {
        list = buffer_list_alloc(size);
        if (list == NULL) {
            return -1;
        }
        arg->buffer_list = arg->buffer_list_last = list;
    }
    list = arg->buffer_list_last;
    rest = list->size - list->used;
    if (size <= rest) {
        memcpy(list->buf + list->used, data, size);
        list->used += size;
    } else {
        memcpy(list->buf + list->used, data, rest);
        list->used += rest;
        data = (const char*)data + rest;
        size -= rest;
        list = buffer_list_alloc(size);
        if (list == NULL) {
            return -1;
        }
        memcpy(list->buf, data, size);
        list->used = size;
        arg->buffer_list_last->next = list;
        arg->buffer_list_last = list;
    }
    return 0;
}

static buffer_list_t *buffer_list_alloc(size_t size)
{
    size_t alloc_size = MAX(size + offsetof(buffer_list_t, buf), MIN_BUFFER_LIST_SIZE);
    buffer_list_t *list = malloc(alloc_size);

    if (list == NULL) {
        return NULL;
    }
    list->next = NULL;
    list->size = alloc_size - offsetof(buffer_list_t, buf);
    list->used = 0;
    return list;
}

static VALUE
flite_s_list_builtin_voices(VALUE klass)
{
    VALUE ary = rb_ary_new();
    const rbflite_builtin_voice_t *builtin = rbflite_builtin_voice_list;

    while (builtin->name != NULL) {
        rb_ary_push(ary, rb_usascii_str_new_cstr(builtin->name));
        builtin++;
    }

    return ary;
}

static void
rbfile_voice_free(rbflite_voice_t *voice)
{
    if (voice->voice) {
        delete_voice(voice->voice);
        voice->voice = NULL;
    }
}

static VALUE
rbflite_voice_s_allocate(VALUE klass)
{
    rbflite_voice_t *voice;
    VALUE obj = Data_Make_Struct(klass, rbflite_voice_t, NULL, rbfile_voice_free, voice);

    voice->queue.tail = &voice->queue.head;
    return obj;
}

#ifdef HAVE_FLITE_VOICE_LOAD
static void *
rbflite_voice_load(void *data)
{
    return flite_voice_load((const char *)data);
}
#endif

static VALUE
rbflite_voice_initialize(int argc, VALUE *argv, VALUE self)
{
    VALUE name;
    const rbflite_builtin_voice_t *builtin = rbflite_builtin_voice_list;
    rbflite_voice_t *voice = DATA_PTR(self);

    rb_scan_args(argc, argv, "01", &name);
    if (!NIL_P(name)) {
        char *voice_name = StringValueCStr(name);
        while (builtin->name != NULL) {
            if (strcmp(voice_name, builtin->name) == 0) {
                break;
            }
            builtin++;
        }
        if (builtin->name == NULL) {
#ifdef HAVE_FLITE_VOICE_LOAD
            if (strchr(voice_name, '/') != NULL || strchr(voice_name, '.') != NULL) {
                voice->voice = rb_thread_call_without_gvl(rbflite_voice_load, voice_name, NULL, NULL);
                RB_GC_GUARD(name);
                if (voice->voice != NULL) {
                    return self;
                }
            }
#endif
            rb_raise(rb_eArgError, "Unkonw voice %s", voice_name);
        }
    }
    *builtin->cached = NULL; /* disable voice caching in libflite.so. */
    voice->voice = builtin->register_(NULL);
    return self;
}

static void *
voice_speech_without_gvl(void *data)
{
    voice_speech_arg_t *arg = (voice_speech_arg_t *)data;
    flite_text_to_speech(arg->text, arg->voice, arg->outtype);
    return NULL;
}

static int
rbflite_audio_write_cb(const cst_wave *w, int start, int size, int last, asc_last_arg_t last_arg)
{
    voice_speech_arg_t *ud = (voice_speech_arg_t *)ASC_LAST_ARG_TO_USERDATA(last_arg);

    if (start == 0) {
        /* write WAVE file header. */
        struct {
            const char riff_id[4];
            int file_size;
            const char wave_id[4];
            const char fmt_id[4];
            const int fmt_size;
            const short format;
            short channels;
            int samplerate;
            int bytepersec;
            short blockalign;
            short bitswidth;
            const char data[4];
            int data_size;
        } header = {
            {'R', 'I', 'F', 'F'},
            0,
            {'W', 'A', 'V', 'E'},
            {'f', 'm', 't', ' '},
            TO_LE4(16),
            TO_LE2(0x0001),
            0, 0, 0, 0, 0,
            {'d', 'a', 't', 'a'},
            0,
        };
        int num_samples = cst_wave_num_samples(w);
        int num_channels = cst_wave_num_channels(w);
        int sample_rate = cst_wave_sample_rate(w);
        int data_size = num_channels * num_samples * sizeof(short);

        header.file_size = TO_LE4(sizeof(header) + data_size - 8);
        header.channels = TO_LE2(num_channels);
        header.samplerate = TO_LE2(sample_rate);
        header.bytepersec = TO_LE4(sample_rate * num_channels * sizeof(short));
        header.blockalign = TO_LE2(num_channels * sizeof(short));
        header.bitswidth = TO_LE2(sizeof(short) * 8);
        header.data_size = TO_LE4(data_size);

        if (add_data(ud, &header, sizeof(header)) != 0) {
            return CST_AUDIO_STREAM_STOP;
        }
    }

    if (add_data(ud, &w->samples[start], size * sizeof(short)) != 0) {
        return CST_AUDIO_STREAM_STOP;
    }
    return CST_AUDIO_STREAM_CONT;
}

static VALUE
rbflite_voice_speak(VALUE self, VALUE text)
{
    rbflite_voice_t *voice = DATA_PTR(self);
    voice_speech_arg_t arg;
    thread_queue_entry_t entry;

    if (voice->voice == NULL) {
        rb_raise(rb_eRuntimeError, "not initialized");
    }

    arg.voice = voice->voice;
    arg.text = StringValueCStr(text);
    arg.outtype = "play";
    arg.buffer_list = NULL;
    arg.buffer_list_last = NULL;

    lock_thread(&voice->queue, &entry);

    rb_thread_call_without_gvl(voice_speech_without_gvl, &arg, NULL, NULL);
    RB_GC_GUARD(text);

    unlock_thread(&voice->queue);

    return self;
}

static VALUE
rbflite_voice_to_speech(VALUE self, VALUE text)
{
    rbflite_voice_t *voice = DATA_PTR(self);
    cst_audio_streaming_info *asi = NULL;
    voice_speech_arg_t arg;
    thread_queue_entry_t entry;
    buffer_list_t *list, *list_next;
    size_t size;
    VALUE speech_data;
    char *ptr;

    if (voice->voice == NULL) {
        rb_raise(rb_eRuntimeError, "not initialized");
    }

    arg.voice = voice->voice;
    arg.text = StringValueCStr(text);
    arg.outtype = "stream";
    arg.buffer_list = NULL;
    arg.buffer_list_last = NULL;

    /* write to an object */
    asi = new_audio_streaming_info();
    if (asi == NULL) {
        rb_raise(rb_eNoMemError, "failed to allocate audio_streaming_info");
    }
    asi->asc = rbflite_audio_write_cb;
    asi->userdata = (void*)&arg;

    lock_thread(&voice->queue, &entry);

    feat_set(voice->voice->features, "streaming_info", audio_streaming_info_val(asi));
    rb_thread_call_without_gvl(voice_speech_without_gvl, &arg, NULL, NULL);
    flite_feat_remove(voice->voice->features, "streaming_info");
    RB_GC_GUARD(text);

    unlock_thread(&voice->queue);

    size = 0;
    for (list = arg.buffer_list; list != NULL; list = list->next) {
        size += list->used;
    }
    speech_data = rb_str_buf_new(size);
    ptr = RSTRING_PTR(speech_data);
    for (list = arg.buffer_list; list != NULL; list = list_next) {
        memcpy(ptr, list->buf, list->used);
        ptr += list->used;
        list_next = list->next;
        free(list);
    }
    rb_str_set_len(speech_data, size);

    return speech_data;
}

static VALUE
rbflite_voice_name(VALUE self)
{
    rbflite_voice_t *voice = DATA_PTR(self);

    if (voice->voice == NULL) {
        rb_raise(rb_eRuntimeError, "not initialized");
    }
    return rb_usascii_str_new_cstr(voice->voice->name);
}

static VALUE
rbflite_voice_pathname(VALUE self)
{
    rbflite_voice_t *voice = DATA_PTR(self);
    const char *pathname;

    if (voice->voice == NULL) {
        rb_raise(rb_eRuntimeError, "not initialized");
    }
    pathname = get_param_string(voice->voice->features, "pathname", "");
    if (pathname[0] == '\0') {
        return Qnil;
    }
    return rb_usascii_str_new_cstr(pathname);
}

void
Init_flite(void)
{
    VALUE cmu_flite_version;

    rb_mFlite = rb_define_module("Flite");

    cmu_flite_version = rb_usascii_str_new_cstr(FLITE_PROJECT_VERSION);
    OBJ_FREEZE(cmu_flite_version);
    rb_define_const(rb_mFlite, "CMU_FLITE_VERSION", cmu_flite_version);

#ifdef HAVE_FLITE_ADD_LANG
#ifdef HAVE_LANG_ENG
    flite_add_lang("eng", usenglish_init, cmulex_init);
    flite_add_lang("usenglish", usenglish_init, cmulex_init);
#endif
#ifdef HAVE_LANG_INDIC
    flite_add_lang("cmu_indic_lang", cmu_indic_lang_init, cmu_indic_lex_init);
#endif
#ifdef HAVE_LANG_GRAPHEME
   flite_add_lang("cmu_grapheme_lang",cmu_grapheme_lang_init,cmu_grapheme_lex_init);
#endif
#endif

    rb_define_singleton_method(rb_mFlite, "list_builtin_voices", flite_s_list_builtin_voices, 0);
    rb_cVoice = rb_define_class_under(rb_mFlite, "Voice", rb_cObject);
    rb_define_alloc_func(rb_cVoice, rbflite_voice_s_allocate);

    rb_define_method(rb_cVoice, "initialize", rbflite_voice_initialize, -1);
    rb_define_method(rb_cVoice, "speak", rbflite_voice_speak, 1);
    rb_define_method(rb_cVoice, "to_speech", rbflite_voice_to_speech, 1);
    rb_define_method(rb_cVoice, "name", rbflite_voice_name, 0);
    rb_define_method(rb_cVoice, "pathname", rbflite_voice_pathname, 0);
}
