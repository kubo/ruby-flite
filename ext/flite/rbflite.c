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

#ifdef HAVE_LAME_LAME_H
#include <lame/lame.h>
#define HAVE_MP3LAME 1
#endif

#ifdef HAVE_LAME_H
#include <lame.h>
#define HAVE_MP3LAME 1
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

enum rbfile_error {
    RBFLITE_ERROR_SUCCESS,
    RBFLITE_ERROR_OUT_OF_MEMORY,
    RBFLITE_ERROR_LAME_INIT_PARAMS,
    RBFLITE_ERROR_LAME_ENCODE_BUFFER,
    RBFLITE_ERROR_LAME_ENCODE_FLUSH,
};

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
    void *encoder;
    buffer_list_t *buffer_list;
    buffer_list_t *buffer_list_last;
    enum rbfile_error error;
} voice_speech_data_t;

typedef struct {
    cst_audio_stream_callback asc;
    void *(*encoder_init)(VALUE opts);
    void (*encoder_fini)(void *encoder);
} audio_stream_encoder_t;

static VALUE rb_mFlite;
static VALUE rb_eFliteError;
static VALUE rb_eFliteRuntimeError;
static VALUE rb_cVoice;
static VALUE sym_mp3;
static VALUE sym_raw;
static VALUE sym_wav;
static struct timeval sleep_time_after_speaking;

static buffer_list_t *buffer_list_alloc(size_t size);
static void check_error(voice_speech_data_t *vsd);

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

static int add_data(voice_speech_data_t *vsd, const void *data, size_t size)
{
    buffer_list_t *list;
    size_t rest;

    if (vsd->buffer_list == NULL) {
        list = buffer_list_alloc(size);
        if (list == NULL) {
            vsd->error = RBFLITE_ERROR_OUT_OF_MEMORY;
            return -1;
        }
        vsd->buffer_list = vsd->buffer_list_last = list;
    }
    list = vsd->buffer_list_last;
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
            vsd->error = RBFLITE_ERROR_OUT_OF_MEMORY;
            return -1;
        }
        memcpy(list->buf, data, size);
        list->used = size;
        vsd->buffer_list_last->next = list;
        vsd->buffer_list_last = list;
    }
    return 0;
}

static buffer_list_t *buffer_list_alloc(size_t size)
{
    size_t alloc_size = MAX(size + offsetof(buffer_list_t, buf), MIN_BUFFER_LIST_SIZE);
    buffer_list_t *list = xmalloc(alloc_size);

    if (list == NULL) {
        return NULL;
    }
    list->next = NULL;
    list->size = alloc_size - offsetof(buffer_list_t, buf);
    list->used = 0;
    return list;
}

static void check_error(voice_speech_data_t *vsd)
{
    buffer_list_t *list, *list_next;

    if (vsd->error == RBFLITE_ERROR_SUCCESS) {
        return;
    }
    for (list = vsd->buffer_list; list != NULL; list = list_next) {
        list_next = list->next;
        xfree(list);
    }
    vsd->buffer_list = NULL;
    switch (vsd->error) {
    case RBFLITE_ERROR_OUT_OF_MEMORY:
        rb_raise(rb_eNoMemError, "out of memory while writing speech data");
    case RBFLITE_ERROR_LAME_INIT_PARAMS:
        rb_raise(rb_eFliteRuntimeError, "lame_init_params() error");
    case RBFLITE_ERROR_LAME_ENCODE_BUFFER:
        rb_raise(rb_eFliteRuntimeError, "lame_encode_buffer() error");
    case RBFLITE_ERROR_LAME_ENCODE_FLUSH:
        rb_raise(rb_eFliteRuntimeError, "lame_encode_flush() error");
    default:
        rb_raise(rb_eFliteRuntimeError, "Unkown error %d", vsd->error);
    }
}

/*
 *  Returns builtin voice names.
 *
 *  @example
 *    Flite.list_builtin_voices # => ["kal", "awb_time", "kal16", "awb", "rms", "slt"]
 *
 *  @return [Array]
 */
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

/*
 *  Returns supported audio types used as the second argument of {Flite::Voice#to_speech}.
 *
 *  @example
 *    # Compiled with mp3 support
 *    Flite.supported_audio_types # => [:wav, :raw, :mp3]
 *
 *    # Compiled without mp3 support
 *    Flite.supported_audio_types # => [:wav, :raw]
 *
 *  @return [Array]
 */
static VALUE
flite_s_supported_audio_types(VALUE klass)
{
    VALUE ary = rb_ary_new();

    rb_ary_push(ary, sym_wav);
    rb_ary_push(ary, sym_raw);
#ifdef HAVE_MP3LAME
    rb_ary_push(ary, sym_mp3);
#endif
    return ary;
}

/*
 * @overload sleep_time_after_speaking=(sec)
 *
 *  Sets sleep time after {Flite::Voice#speak}.
 *  The default value is 0 on Unix and 0.3 on Windows.
 *
 *  This is workaround for voice cutoff on Windows.
 *  The following code speaks "Hello Wor.. Hello World" without
 *  0.3 seconds sleep.
 *
 *      "Hello World".speak # The last 0.3 seconds are cut off by the next speech on Windows.
 *      "Hello World".speak
 *
 *  @param [Float] sec seconds to sleep
 */
static VALUE
flite_s_set_sleep_time_after_speaking(VALUE klass, VALUE val)
{
    sleep_time_after_speaking = rb_time_interval(val);
    return val;
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

/*
 * @overload initialize(name = nil)
 *
 *  Create a new voice specified by <code>name</code>.
 *  If <code>name</code> includes '.' or '/' and ruby flite
 *  is compiled for CMU Flite 2.0.0 or upper, try to
 *  use a loadable voice.
 *
 *  @example
 *
 *    # Use default voice. It is 'kal' usually.
 *    voice = Flite::Voice.new
 *
 *    # Use a builtin voice.
 *    voice = Flite::Voice.new('awb')
 *
 *    # Use a lodable voice.
 *    voice = Flite::Voice.new('/path/to/cmu_us_gka.flitevox')
 *
 *  @param [String] name
 *  @see Flite.list_builtin_voices
 */
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
    voice_speech_data_t *vsd = (voice_speech_data_t *)data;
    flite_text_to_speech(vsd->text, vsd->voice, vsd->outtype);
    return NULL;
}

static int
wav_encoder_cb(const cst_wave *w, int start, int size, int last, asc_last_arg_t last_arg)
{
    voice_speech_data_t *vsd = (voice_speech_data_t *)ASC_LAST_ARG_TO_USERDATA(last_arg);

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

        if (add_data(vsd, &header, sizeof(header)) != 0) {
            return CST_AUDIO_STREAM_STOP;
        }
    }

    if (add_data(vsd, &w->samples[start], size * sizeof(short)) != 0) {
        return CST_AUDIO_STREAM_STOP;
    }
    return CST_AUDIO_STREAM_CONT;
}

static audio_stream_encoder_t wav_encoder = {
    wav_encoder_cb,
    NULL,
    NULL,
};

static int
raw_encoder_cb(const cst_wave *w, int start, int size, int last, asc_last_arg_t last_arg)
{
    voice_speech_data_t *vsd = (voice_speech_data_t *)ASC_LAST_ARG_TO_USERDATA(last_arg);

    if (add_data(vsd, &w->samples[start], size * sizeof(short)) != 0) {
        return CST_AUDIO_STREAM_STOP;
    }
    return CST_AUDIO_STREAM_CONT;
}

static audio_stream_encoder_t raw_encoder = {
    raw_encoder_cb,
    NULL,
    NULL,
};

#ifdef HAVE_MP3LAME

#define MAX_SAMPLE_SIZE 1024
/* "mp3buf_size in bytes = 1.25*num_samples + 7200" according to lame.h. */
#define MP3BUF_SIZE  (MAX_SAMPLE_SIZE + MAX_SAMPLE_SIZE / 4 + 7200)
static int mp3_encoder_cb(const cst_wave *w, int start, int size, int last, asc_last_arg_t last_arg)
{
    voice_speech_data_t *vsd = (voice_speech_data_t *)ASC_LAST_ARG_TO_USERDATA(last_arg);
    lame_global_flags *gf = vsd->encoder;
    unsigned char mp3buf[MP3BUF_SIZE];
    short *sptr = &w->samples[start];
    short *eptr = sptr + size;
    int rv;

    if (start == 0) {
        lame_set_num_samples(gf, cst_wave_num_samples(w));
        lame_set_in_samplerate(gf, cst_wave_sample_rate(w));
        lame_set_num_channels(gf, 1);
        lame_set_mode(gf, MONO);
        rv = lame_init_params(gf);
        if (rv == -1) {
            vsd->error = RBFLITE_ERROR_LAME_INIT_PARAMS;
            return CST_AUDIO_STREAM_STOP;
        }
    }
    while (eptr - sptr > MAX_SAMPLE_SIZE) {
        rv = lame_encode_buffer(gf, sptr, NULL, MAX_SAMPLE_SIZE, mp3buf, sizeof(mp3buf));
        if (rv < 0) {
            vsd->error = RBFLITE_ERROR_LAME_ENCODE_BUFFER;
            return CST_AUDIO_STREAM_STOP;
        }
        if (rv > 0) {
            if (add_data(vsd, mp3buf, rv) != 0) {
                return CST_AUDIO_STREAM_STOP;
            }
        }
        sptr += MAX_SAMPLE_SIZE;
    }
    rv = lame_encode_buffer(gf, sptr, NULL, eptr - sptr, mp3buf, sizeof(mp3buf));
    if (rv < 0) {
        vsd->error = RBFLITE_ERROR_LAME_ENCODE_BUFFER;
        return CST_AUDIO_STREAM_STOP;
    }
    if (rv > 0) {
        if (add_data(vsd, mp3buf, rv) != 0) {
            return CST_AUDIO_STREAM_STOP;
        }
    }
    if (last) {
        rv = lame_encode_flush(gf, mp3buf, sizeof(mp3buf));
        if (rv < 0) {
            vsd->error = RBFLITE_ERROR_LAME_ENCODE_FLUSH;
            return CST_AUDIO_STREAM_STOP;
        }
        if (rv > 0) {
            if (add_data(vsd, mp3buf, rv) != 0) {
                return CST_AUDIO_STREAM_STOP;
            }
        }
    }
    return CST_AUDIO_STREAM_CONT;
}

static void *mp3_encoder_init(VALUE opts)
{
    lame_global_flags *gf = lame_init();

    if (gf == NULL) {
        rb_raise(rb_eFliteRuntimeError, "Failed to initialize lame");
    }

    lame_set_bWriteVbrTag(gf, 0);
    lame_set_brate(gf, 64);

    if (!NIL_P(opts)) {
        VALUE v;
        Check_Type(opts, T_HASH);

        v = rb_hash_aref(opts, ID2SYM(rb_intern("bitrate")));
        if (!NIL_P(v)) {
            lame_set_brate(gf, NUM2INT(v));
        }

        v = rb_hash_aref(opts, ID2SYM(rb_intern("scale")));
        if (!NIL_P(v)) {
            lame_set_scale(gf, NUM2INT(v));
        }

        v = rb_hash_aref(opts, ID2SYM(rb_intern("quality")));
        if (!NIL_P(v)) {
            lame_set_quality(gf, NUM2INT(v));
        }
    }

    lame_set_bWriteVbrTag(gf, 0);
    return gf;
}

static void mp3_encoder_fini(void *encoder)
{
    lame_close(encoder);
}

static audio_stream_encoder_t mp3_encoder = {
    mp3_encoder_cb,
    mp3_encoder_init,
    mp3_encoder_fini,
};

#endif

/*
 * @overload speak(text)
 *
 *  Speak the <code>text</code>.
 *
 *  @example
 *    voice = Flite::Voice.new
 *
 *    # Speak 'Hello Flite World!'
 *    voice.speak('Hello Flite World!')
 *
 *  @param [String] text
 */
static VALUE
rbflite_voice_speak(VALUE self, VALUE text)
{
    rbflite_voice_t *voice = DATA_PTR(self);
    voice_speech_data_t vsd;
    thread_queue_entry_t entry;

    if (voice->voice == NULL) {
        rb_raise(rb_eFliteRuntimeError, "%s is not initialized", rb_obj_classname(self));
    }

    vsd.voice = voice->voice;
    vsd.text = StringValueCStr(text);
    vsd.outtype = "play";
    vsd.buffer_list = NULL;
    vsd.buffer_list_last = NULL;
    vsd.error = RBFLITE_ERROR_SUCCESS;

    lock_thread(&voice->queue, &entry);

    rb_thread_call_without_gvl(voice_speech_without_gvl, &vsd, NULL, NULL);
    RB_GC_GUARD(text);

    unlock_thread(&voice->queue);

    check_error(&vsd);

    if (sleep_time_after_speaking.tv_sec != 0 || sleep_time_after_speaking.tv_usec != 0) {
        rb_thread_wait_for(sleep_time_after_speaking);
    }

    return self;
}

/*
 * @overload to_speech(text, audio_type = :wav, opts = {})
 *
 *  Converts <code>text</code> to audio data.
 *
 *  @example
 *    voice = Flite::Voice.new
 *
 *    # Save speech as wav
 *    File.binwrite('hello_flite_world.wav',
 *                  voice.to_speech('Hello Flite World!'))
 *
 *    # Save speech as raw pcm (signed 16 bit little endian, rate 8000 Hz, mono)
 *    File.binwrite('hello_flite_world.raw',
 *                  voice.to_speech('Hello Flite World!', :raw))
 *
 *    # Save speech as mp3
 *    File.binwrite('hello_flite_world.mp3',
 *                  voice.to_speech('Hello Flite World!', :mp3))
 *
 *    # Save speech as mp3 whose bitrate is 128k.
 *    File.binwrite('hello_flite_world.mp3',
 *                  voice.to_speech('Hello Flite World!', :mp3, :bitrate => 128))
 *
 *  @param [String] text
 *  @param [Symbol] audo_type :wav, :raw or :mp3 (when mp3 support is enabled)
 *  @param [Hash]   opts  audio encoder options
 *  @return [String] audio data
 *  @see Flite.supported_audio_types
 */
static VALUE
rbflite_voice_to_speech(int argc, VALUE *argv, VALUE self)
{
    rbflite_voice_t *voice = DATA_PTR(self);
    VALUE text;
    VALUE audio_type;
    VALUE opts;
    cst_audio_streaming_info *asi = NULL;
    audio_stream_encoder_t *encoder;
    voice_speech_data_t vsd;
    thread_queue_entry_t entry;
    buffer_list_t *list, *list_next;
    size_t size;
    VALUE speech_data;
    char *ptr;

    if (voice->voice == NULL) {
        rb_raise(rb_eFliteRuntimeError, "%s is not initialized", rb_obj_classname(self));
    }

    rb_scan_args(argc, argv, "12", &text, &audio_type, &opts);

    if (NIL_P(audio_type)) {
        encoder = &wav_encoder;
    } else {
        if (rb_equal(audio_type, sym_wav)) {
            encoder = &wav_encoder;
        } else if (rb_equal(audio_type, sym_raw)) {
            encoder = &raw_encoder;
#ifdef HAVE_MP3LAME
        } else if (rb_equal(audio_type, sym_mp3)) {
            encoder = &mp3_encoder;
#endif
        } else {
            rb_raise(rb_eArgError, "unknown audio type");
        }
    }

    vsd.voice = voice->voice;
    vsd.text = StringValueCStr(text);
    vsd.outtype = "stream";
    vsd.encoder = NULL;
    vsd.buffer_list = NULL;
    vsd.buffer_list_last = NULL;
    vsd.error = RBFLITE_ERROR_SUCCESS;

    if (encoder->encoder_init) {
        vsd.encoder = encoder->encoder_init(opts);
    }

    /* write to an object */
    asi = new_audio_streaming_info();
    if (asi == NULL) {
        if (encoder->encoder_fini) {
            encoder->encoder_fini(vsd.encoder);
        }
        rb_raise(rb_eNoMemError, "failed to allocate audio_streaming_info");
    }
    asi->asc = encoder->asc;
    asi->userdata = (void*)&vsd;

    lock_thread(&voice->queue, &entry);

    flite_feat_set(voice->voice->features, "streaming_info", audio_streaming_info_val(asi));
    rb_thread_call_without_gvl(voice_speech_without_gvl, &vsd, NULL, NULL);
    flite_feat_remove(voice->voice->features, "streaming_info");
    RB_GC_GUARD(text);

    unlock_thread(&voice->queue);

    if (encoder->encoder_fini) {
        encoder->encoder_fini(vsd.encoder);
    }

    check_error(&vsd);

    size = 0;
    for (list = vsd.buffer_list; list != NULL; list = list->next) {
        size += list->used;
    }
    speech_data = rb_str_buf_new(size);
    ptr = RSTRING_PTR(speech_data);
    for (list = vsd.buffer_list; list != NULL; list = list_next) {
        memcpy(ptr, list->buf, list->used);
        ptr += list->used;
        list_next = list->next;
        xfree(list);
    }
    rb_str_set_len(speech_data, size);

    return speech_data;
}

/*
 * @overload name
 *
 *  Returns voice name.
 *
 *  @example
 *    voice = Flite::Voice.new('slt')
 *    voice.name => 'slt'
 *
 *    # voice loading is a new feature of CMU Flite 2.0.0.
 *    voice = Flite::Voice.new('/path/to/cmu_us_fem.flitevox')
 *    voice.name => 'cmu_us_fem'
 *
 *  @return [String]
 */
static VALUE
rbflite_voice_name(VALUE self)
{
    rbflite_voice_t *voice = DATA_PTR(self);

    if (voice->voice == NULL) {
        rb_raise(rb_eFliteRuntimeError, "%s is not initialized", rb_obj_classname(self));
    }
    return rb_usascii_str_new_cstr(voice->voice->name);
}

/*
 * @overload pathname
 *
 *  Returns the path of the voice if the voice is a loadable voice.
 *  Otherwise, nil.
 *
 *  @example
 *    voice = Flite::Voice.new
 *    voice.pathname => 'kal'
 *
 *    # voice loading is a new feature of CMU Flite 2.0.0.
 *    voice = Flite::Voice.new('/path/to/cmu_us_aup.flitevox')
 *    voice.pathname => '/path/to/cmu_us_aup.flitevox'
 *
 *  @return [String]
 */
static VALUE
rbflite_voice_pathname(VALUE self)
{
    rbflite_voice_t *voice = DATA_PTR(self);
    const char *pathname;

    if (voice->voice == NULL) {
        rb_raise(rb_eFliteRuntimeError, "%s is not initialized", rb_obj_classname(self));
    }
    pathname = flite_get_param_string(voice->voice->features, "pathname", "");
    if (pathname[0] == '\0') {
        return Qnil;
    }
    return rb_usascii_str_new_cstr(pathname);
}

/*
 * @overload inspect
 *
 *  Returns the value as a string for inspection.
 *
 *  @return [String]
 *  @private
 */
static VALUE
rbflite_voice_inspect(VALUE self)
{
    rbflite_voice_t *voice = DATA_PTR(self);
    const char *class_name = rb_obj_classname(self);
    const char *voice_name;
    const char *pathname;

    if (voice->voice == NULL) {
        return rb_sprintf("#<%s: not initialized>", class_name);
    }
    voice_name = voice->voice->name;

    pathname = flite_get_param_string(voice->voice->features, "pathname", "");
    if (pathname[0] == '\0') {
        return rb_sprintf("#<%s: %s>", class_name, voice_name);
    } else {
        return rb_sprintf("#<%s: %s (%s)>", class_name, voice_name, pathname);
    }
}

#ifdef _WIN32
__declspec(dllexport) void Init_flite(void);
#endif

void
Init_flite(void)
{
    VALUE cmu_flite_version;

    sym_mp3 = ID2SYM(rb_intern("mp3"));
    sym_raw = ID2SYM(rb_intern("raw"));
    sym_wav = ID2SYM(rb_intern("wav"));

    rb_mFlite = rb_define_module("Flite");
    rb_eFliteError = rb_define_class_under(rb_mFlite, "Error", rb_eStandardError);
    rb_eFliteRuntimeError = rb_define_class_under(rb_mFlite, "Runtime", rb_eFliteError);

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
    rb_define_singleton_method(rb_mFlite, "supported_audio_types", flite_s_supported_audio_types, 0);
    rb_define_singleton_method(rb_mFlite, "sleep_time_after_speaking=", flite_s_set_sleep_time_after_speaking, 1);
    rb_cVoice = rb_define_class_under(rb_mFlite, "Voice", rb_cObject);
    rb_define_alloc_func(rb_cVoice, rbflite_voice_s_allocate);

    rb_define_method(rb_cVoice, "initialize", rbflite_voice_initialize, -1);
    rb_define_method(rb_cVoice, "speak", rbflite_voice_speak, 1);
    rb_define_method(rb_cVoice, "to_speech", rbflite_voice_to_speech, -1);
    rb_define_method(rb_cVoice, "name", rbflite_voice_name, 0);
    rb_define_method(rb_cVoice, "pathname", rbflite_voice_pathname, 0);
    rb_define_method(rb_cVoice, "inspect", rbflite_voice_inspect, 0);
}
