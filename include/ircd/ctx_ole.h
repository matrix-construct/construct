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
 *
 */

#pragma once
#define HAVE_IRCD_CTX_OLE_H

// Context OffLoad Engine (OLE)
//
// This system allows moving a task off the main IRCd thread by passing a function to a
// worker thread for execution. The context on the main IRCd thread yields until the offload
// function has returned (or thrown). Please read the last paragraph and be thoroughly dissuaded
// from temptation to abuse this.
//
// The offload engine is for ye 'ole libraries or opaque function calls which may hang the
// program with blocking I/O, extreme and time-consuming computation, or other uncooperative
// behavior. IRCd is, at its core, still a single-threaded asynchronous environment and while
// userspace context switching is provided to eliminate callbacks, the same rules for hanging
// the program still apply.
//
// Before using this system, do everything you possibly can to figure out if you can participate
// directly in the main boost::asio (glorified epoll()) system. If you have access to a file
// descriptor, socket, or similar device exposed by the library YOU DO NOT NEED THIS SYSTEM.
// We do not *prefer* to use threads just to have them wait on I/O. This system is also not for
// moderately expensive computation; it is alright to be fairly liberal with the main thread.
// You must factor in the round trip costs of kernel thread context switching, cache/bus
// transfers and contention, and whether any concurrency will really even be produced at all
// to make a good case for an offload that doesn't hurt more than it helps. And when it hurts,
// it hurts _everything_.
//
namespace ircd {
namespace ctx  {
namespace ole  {

void offload(const std::function<void ()> &);

struct init
{
	init();
	~init() noexcept;
};

} // namespace ole

using ole::offload;

} // namespace ctx
} // namespace ircd
