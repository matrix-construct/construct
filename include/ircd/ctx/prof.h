/*
 * charybdis: oh just a little chat server
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
#define HAVE_IRCD_CTX_PROF_H

/* Profiling for the context system. These facilities provide tools and statistics.
 * The primary purpose here is to alert developers of unwanted context behavior, in
 * addition to optimizing the overall performance of the context system.
 *
 * The original use case is for the embedded database backend. Function calls are made which may
 * conduct blocking I/O before returning. This will hang the current userspace context while it
 * is running and thus BLOCK EVERY CONTEXT in the entire IRCd. Since this is still an asynchronous
 * system, it just doesn't have callbacks: we do not do I/O without a cooperative yield.
 * Fortunately there are mechanisms to mitigate this -- but we have to know for sure. A database
 * call which has been passed over for mitigation may start doing some blocking flush under load,
 * etc. The profiler will alert us of this so it doesn't silently degrade performance.
 */
namespace ircd::ctx::prof
{
	enum class event;
	struct settings extern settings;

	void mark(const event &);
}

enum class ircd::ctx::prof::event
{
	SPAWN,             // Context spawn requested
	JOIN,              // Context join requested
	JOINED,            // Context join completed
	CUR_ENTER,         // Current context entered
	CUR_LEAVE,         // Current context leaving
	CUR_YIELD,         // Current context yielding
	CUR_CONTINUE,      // Current context continuing
	CUR_INTERRUPT,     // Current context detects interruption
};

struct ircd::ctx::prof::settings
{
	double stack_usage_warning;       // percentage
	double stack_usage_assertion;     // percentage

	microseconds slice_warning;       // Warn when the yield-to-yield time exceeds
	microseconds slice_interrupt;     // Interrupt exception when exceeded (not a signal)
	microseconds slice_assertion;     // abort() when exceeded (not a signal, must yield)
};
