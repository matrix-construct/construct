/*
 * Copyright (C) 2016 Charybdis Development Team
 * Copyright (C) 2016 Jason Volk <jason@zemos.net>
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice is present in all copies.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING
 * IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once
#define HAVE_IRCD_JS_DEBUG_H

namespace ircd {
namespace js   {

// Returns static string
const char *reflect(const JSType &);
const char *reflect(const JSExnType &);
const char *reflect(const JSGCStatus &);
const char *reflect(const JSGCParamKey &);
const char *reflect(const JSFinalizeStatus &);
const char *reflect(const JSContextOp &);
const char *reflect_prop(const uint &flag);
const char *reflect_telemetry(const int &id);
const char *reflect(const jstype &);

// Returns single-line string
std::string debug(const JS::Value &);
std::string debug(const JS::HandleObject &);
std::string debug(const JSErrorReport &);
std::string debug(const JSTracer &);

// prints to IRCd stdout
void dump(const JSString *const &v);
void dump(const JSAtom *const &v);
void dump(const JSObject *const &v);
void dump(const JS::Value &v);
void dump(const jsid &v);
void dump(const JSContext *v);
void dump(const JSScript *const &v);
void dump(const char16_t *const &v, const size_t &len);
void dump(const ::js::InterpreterFrame *v);
void backtrace();

// writes lines to ircd::js::log
void log_gcparams();

} // namespace js
} // namespace ircd
