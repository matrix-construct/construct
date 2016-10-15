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
#define HAVE_IRCD_JS_RUNTIME_H

namespace ircd {
namespace js   {

class runtime
:custom_ptr<JSRuntime>
{
	static void handle_error(JSContext *, const char *msg, JSErrorReport *);
	static void handle_out_of_memory(JSContext *, void *);
	static void handle_large_allocation_failure(void *);
	static void handle_gc(JSRuntime *, JSGCStatus, void *);
	static void handle_finalize(JSFreeOp *, JSFinalizeStatus, bool is_compartment, void *);
	static void handle_destroy_compartment(JSFreeOp *, JSCompartment *);
	static void handle_iterate_compartments(JSRuntime *, void *, JSCompartment *);
	static bool handle_context(JSContext *, uint op, void *);
	static bool handle_interrupt(JSContext *);

  public:
	struct opts
	{
		size_t maxbytes              = 64_MiB;
		size_t code_stack_max        = 0;
		size_t trusted_stack_max     = 0;
		size_t untrusted_stack_max   = 0;
	};

	struct opts opts;                            // We keep a copy of the given opts here

	operator JSRuntime *() const                 { return get();                                   }
	operator JSRuntime &() const                 { return custom_ptr<JSRuntime>::operator*();      }
	bool operator!() const                       { return !custom_ptr<JSRuntime>::operator bool(); }
	auto ptr() const                             { return get();                                   }
	auto ptr()                                   { return get();                                   }

	runtime(const struct opts &);
	runtime() = default;
	runtime(runtime &&) noexcept;
	runtime(const runtime &) = delete;
	runtime &operator=(runtime &&) noexcept;
	runtime &operator=(const runtime &) = delete;
	~runtime() noexcept;

	friend void interrupt(runtime &);
};

// Current thread_local runtime. Runtimes/Contexts are single-threaded and we maintain
// one per thread (or only one will be relevant on the given thread you are on).
// Do not construct more than one runtime on the same thread- this is overwritten.
extern __thread runtime *rt;

// Main JSRuntime instance. It is only valid while the js::init object is held by ircd::main().
// This is available to always find main for whatever reason, but use *rt instead.
extern runtime main;

// Get to our `struct runtime` from any upstream JSRuntime
const runtime &our(const JSRuntime *const &);
runtime &our(JSRuntime *const &);

// Get to our runtime from any JSObject
const runtime &object_runtime(const JSObject &);
runtime &object_runtime(JSObject &);

void interrupt(runtime &r);
void run_gc(runtime &r);


inline void
run_gc(runtime &r)
{
	JS_GC(r);
}

inline void
interrupt(runtime &r)
{
	JS_SetInterruptCallback(r, runtime::handle_interrupt);
	JS_RequestInterruptCallback(r);
}

inline runtime &
object_runtime(JSObject &o)
{
	return our(JS_GetObjectRuntime(&o));
}

inline const runtime &
object_runtime(const JSObject &o)
{
	return our(JS_GetObjectRuntime(const_cast<JSObject *>(&o)));
}

inline runtime &
our(JSRuntime *const &c)
{
	return *static_cast<runtime *>(JS_GetRuntimePrivate(c));
}

inline const runtime &
our(const JSRuntime *const &c)
{
	return *static_cast<const runtime *>(JS_GetRuntimePrivate(const_cast<JSRuntime *>(c)));
}


} // namespace js
} // namespace ircd
