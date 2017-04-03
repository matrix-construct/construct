/*
 * charybdis: oh just a little chat server
 * ctx.h: userland context switching (stackful coroutines)
 *
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
#define HAVE_IRCD_CTX_H

//
// This is the public interface to the userspace context system. No 3rd party
// symbols are included from here. This file is included automatically in stdinc.h
// and you do not have to include it manually.
//
// There are two primary objects at work in the context system:
//
// `struct context` <ircd/ctx/context.h>
// Public interface emulating std::thread; included automatically from here.
// To spawn and manipulate contexts, deal with this object.
//
// `struct ctx` (ircd/ctx.cc)
// Internal implementation of the context. This is not included here.
// Several low-level functions are exposed for library creators. This file is usually
// included when boost/asio.hpp is also included and calls are actually made into boost.
//

namespace ircd {
namespace ctx  {

using std::chrono::steady_clock;
using time_point = steady_clock::time_point;

IRCD_EXCEPTION(ircd::error, error)
IRCD_EXCEPTION(error, interrupted)
IRCD_EXCEPTION(error, timeout)

} // namespace ctx
} // namespace ircd

#include "ctx/ctx.h"
#include "ctx/context.h"
#include "ctx/prof.h"
#include "ctx/dock.h"
#include "ctx/queue.h"
#include "ctx/mutex.h"
#include "ctx/shared_state.h"
#include "ctx/promise.h"
#include "ctx/future.h"
#include "ctx/async.h"
#include "ctx/pool.h"
#include "ctx/ole.h"

namespace ircd {

using ctx::timeout;
using ctx::context;
using ctx::sleep;

} // namespace ircd
