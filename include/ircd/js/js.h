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

/* This header exposes the SpiderMonkey engine API to all ye who include it.
 * It also anchors all of our own developed headers and utils which use and extend their API.
 *
 * This header is included after the 'stdinc header' <ircd/js.h>. That header creates the
 * namespace ircd::js, and does not include any 3rd party symbols. <ircd/js.h> is included
 * automatically in stdinc.h.
 *
 * This header should be included if you intend to get dirty with the JS engine subsystem
 * which requires jsapi support and can't be satisfied by the stdinc header.
 */

#pragma once
#define HAVE_IRCD_JS_JS_H

// SpiderMonkey headers require an include basis e.g. -I/usr/include/mozjs-XX as their
// include directives are written as "jsxxx.h" or "mozilla/xxx.h" etc. Our includes are all
// <ircd/xxx.h> and shouldn't have any conflict issues.
#include <jsapi.h>
#include <js/Conversions.h>

// Some forward declarations for jsapi items not declared in the above includes,
// but visible to definition files making use of additional jsapi interfaces.
struct JSAtom;
namespace js {  struct InterpreterFrame;  }

namespace ircd {
namespace js   {

// Extend this class to store your data with any priv(),
// i.e priv(runtime, privdata*) or priv(context, privdata*) etc
struct privdata
{
	virtual ~privdata() noexcept = 0;            // Your object is managed by the host
};

inline const char *version(const JSVersion &v)   { return JS_VersionToString(v);                   }
inline JSVersion version(const char *const &v)   { return JS_StringToVersion(v);                   }

} // namespace js
} // namespace ircd

#include "runtime.h"
#include "timer.h"
#include "context.h"
#include "compartment.h"
#include "debug.h"
#include "error.h"
#include "native.h"
#include "root.h"
#include "value.h"
#include "string.h"
#include "id.h"
#include "object.h"
#include "has.h"
#include "get.h"
#include "set.h"
#include "del.h"
#include "vector.h"
#include "script.h"
#include "function.h"
#include "function_literal.h"
#include "call.h"
#include "for_each.h"
#include "args.h"
#include "trap.h"
#include "trap_function.h"
#include "generator.h"
#include "global.h"
#include "task.h"
