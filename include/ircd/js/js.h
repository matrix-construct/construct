// Matrix Construct
//
// Copyright (C) Matrix Construct Developers, Authors & Contributors
// Copyright (C) 2016-2018 Jason Volk <jason@zemos.net>
//
// Permission to use, copy, modify, and/or distribute this software for any
// purpose with or without fee is hereby granted, provided that the above
// copyright notice and this permission notice is present in all copies. The
// full license for this software is available in the LICENSE file.

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
#ifdef RB_ENABLE_JS
	#define HAVE_IRCD_JS_JS_H

// SpiderMonkey makes use of the `DEBUG` define in headers which must match what the bottom
// end was also compiled with. We tie that define to RB_DEBUG controlled by --enable-debug.
// From a completely clean build, configuring IRCd with --enable-debug should compile SpiderMonkey
// in debug as well.
#ifdef RB_DEBUG
	#define DEBUG
#endif

// SpiderMonkey headers require an include basis e.g. -I/usr/include/mozjs-XX as their
// include directives are written as "jsxxx.h" or "mozilla/xxx.h" etc. Our includes are all
// <ircd/xxx.h> and shouldn't have any conflict issues.
#include <RB_INC_JSAPI_H
#include <RB_INC_JSFRIENDAPI_H
#include <RB_INC_JS_CONVERSIONS_H

// Some forward declarations for jsapi items not declared in the above includes,
// but visible to definition files making use of additional jsapi interfaces.
//struct JSAtom;
//namespace js {  struct InterpreterFrame;  }

namespace ircd {
namespace js   {

// The ostream operator is explicitly brought from ircd:: to compete for efficient overloading,
// and prevent any unnecessary implicit conversions here.
using ircd::operator<<;

} // namespace js
} // namespace ircd

#include "version.h"
#include "type.h"
#include "debug.h"
#include "tracing.h"
#include "timer.h"
#include "context.h"
#include "priv.h"
#include "compartment.h"
#include "root.h"
#include "error.h"
#include "native.h"
#include "value.h"
#include "string.h"
#include "json.h"
#include "id.h"
#include "object.h"
#include "has.h"
#include "get.h"
#include "set.h"
#include "del.h"
#include "vector.h"
#include "for_each.h"
#include "script.h"
#include "module.h"
#include "args.h"
#include "function.h"
#include "function_literal.h"
#include "function_native.h"
#include "call.h"
#include "for_each.h"
#include "trap.h"
#include "trap_function.h"
#include "trap_property.h"
#include "ctor.h"
#include "global.h"
#include "task.h"

#endif // defined(RB_ENABLE_JS)
