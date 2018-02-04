// Matrix Construct
//
// Copyright (C) Matrix Construct Developers, Authors & Contributors
// Copyright (C) 2016-2018 Jason Volk <jason@zemos.net>
//
// Permission to use, copy, modify, and/or distribute this software for any
// purpose with or without fee is hereby granted, provided that the above
// copyright notice and this permission notice is present in all copies. The
// full license for this software is available in the LICENSE file.

#pragma once
#define HAVE_IRCD_JS_DEBUG_H

namespace ircd {
namespace js   {

// Returns static string
const char *reflect(const jstype &);
const char *reflect(const JSType &);
const char *reflect(const JSExnType &);
const char *reflect(const JSGCMode &);
const char *reflect(const JSGCStatus &);
const char *reflect(const JS::GCProgress &);
const char *reflect(const JSGCParamKey &);
const char *reflect(const JSFinalizeStatus &);
const char *reflect(const JS::PromiseState &);
const char *reflect(const PromiseRejectionHandlingState &);
const char *reflect_prop(const uint &flag);
const char *reflect_telemetry(const int &id);

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
void dump_promise(const JS::HandleObject &promise);
void backtrace();

// writes lines to ircd::js::log
void log_gcparams();

} // namespace js
} // namespace ircd
