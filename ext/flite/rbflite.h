/* -*- c-file-style: "ruby"; indent-tabs-mode: nil -*-
 *
 * ruby-flite  -  a small speech synthesis library
 *   https://github.com/kubo/ruby-flite
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
#ifndef RBFLITE_H
#define RBFLITE_H 1

#include "flite/flite.h"

typedef struct {
    const char *name;
    cst_voice *(*register_)(const char *voxdir);
    cst_voice **cached;
#ifdef RBFLITE_WIN32_BINARY_GEM
    const char *dll_name;
    const char *func_name;
    const char *var_name;
#endif
} rbflite_builtin_voice_t;

#ifdef RBFLITE_WIN32_BINARY_GEM
cst_voice *rbfile_call_voice_register_func(rbflite_builtin_voice_t *v, const char *voxdir);
extern rbflite_builtin_voice_t rbflite_builtin_voice_list[];
#else
extern const rbflite_builtin_voice_t rbflite_builtin_voice_list[];
#endif

#endif /* RBFLITE_H */
