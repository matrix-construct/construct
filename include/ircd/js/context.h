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
#define HAVE_IRCD_JS_CONTEXT_H

namespace ircd {
namespace js   {

// Indicates the phase of execution of the javascript
enum class phase
:uint8_t
{
	ACCEPT,        // JS is not running.
	ENTER,         // JS is currently executing or is committed to being entered.
	INTR,          // An interrupt request has or is committed to being sent.
};

// Indicates what operation the interrupt is for
enum class irq
:uint8_t
{
	NONE,          // Sentinel value (no interrupt) (spurious)
	JS,            // JS itself triggers an interrupt after data init before code exec.
	USER,          // User interrupts to have handler (on_intr) called.
	YIELD,         // An ircd::ctx yield should take place, then javascript continues.
	TERMINATE,     // The javascript should be terminated.
};

struct context
:private custom_ptr<JSContext>
{
	// Context options
	struct opts
	{
		size_t stack_chunk_size       = 8_KiB;
		microseconds timer_limit      = 10ms;
	}
	opts;

	// Exception state
	JSExceptionState *except;                    // Use save_exception()/restore_exception()
	JSErrorReport report;                        // Note: ptrs may not be valid in here.

	// Interruption state
	struct alignas(8) state
	{
		uint32_t sem;
		enum phase phase;
		enum irq irq;
	};
	std::atomic<struct state> state;             // Atomic state of execution
	std::function<int (const irq &)> on_intr;    // User interrupt hook (ret -1 to not interfere)
	bool handle_interrupt();                     // Called by runtime on interrupt

	// Execution timer
	void handle_timeout() noexcept;              // Called by timer after requested time
	struct timer timer;

	// JSContext
	operator JSContext *() const                 { return get();                                   }
	operator JSContext &() const                 { return custom_ptr<JSContext>::operator*();      }
	bool operator!() const                       { return !custom_ptr<JSContext>::operator bool(); }
	auto &runtime() const                        { return our(JS_GetRuntime(get()));               }
	auto &runtime()                              { return our(JS_GetRuntime(get()));               }
	auto ptr() const                             { return get();                                   }
	auto ptr()                                   { return get();                                   }

	// BasicLockable                             // std::lock_guard<context>
	void lock()                                  { JS_BeginRequest(get());                         }
	void unlock()                                { JS_EndRequest(get());                           }

	context(struct runtime &, const struct opts &);
	context(const struct opts &);
	context() = default;
	context(context &&) = delete;
	context(const context &) = delete;
	~context() noexcept;
};

// Current thread_local context. Runtimes/Contexts (soon to be merged in future SpiderMonkey)
// are singled-threaded and this points to the context appropos your thread.
// Do not construct more than one context on the same thread- this is overwritten.
extern __thread context *cx;

// Get to our `struct context` from any upstream JSContext
const context &our(const JSContext *const &);
context &our(JSContext *const &);

// Get/Set your privdata managed by this object, casting to your expected type.
template<class T = privdata> const T *priv(const context &);
template<class T = privdata> T *priv(context &);
void priv(context &, privdata *const &);

// Misc
inline auto running(const context &c)            { return JS_IsRunning(c);                         }
inline auto version(const context &c)            { return version(JS_GetVersion(c));               }
JSCompartment *current_compartment(context &);
JSCompartment *current_compartment();
JSObject *current_global(context &c);
JSObject *current_global();                      // thread_local
JS::Zone *current_zone(context &);
JS::Zone *current_zone();

// Memory
void set(context &c, const JSGCParamKey &, const uint32_t &val);
uint32_t get(context &c, const JSGCParamKey &);
void out_of_memory(context &c);
void allocation_overflow(context &c);
void run_gc(context &c);

// Exception
bool pending_exception(const context &c);
void save_exception(context &c, const JSErrorReport &);
bool restore_exception(context &c);
bool report_exception(context &c);

// Interruption
bool interrupt(context &, const irq &);
bool interrupt_poll(const context &c);

// Execution
void restore_frame_chain(context &);
void save_frame_chain(context &);
void enter(context &);  // throws if can't enter
void leave(context &);  // must be called if enter() succeeds

// Enter JS within this closure. Most likely your function will return a `struct value`
// or JS::Value returned by most calls into JS.
template<class F> auto run(F&& function);


template<class F>
auto
run(F&& function)
{
	assert(!pending_exception(*cx));

	enter(*cx);
	const scope out([]
	{
		leave(*cx);
	});

	return function();
}

inline void
save_frame_chain(context &c)
{
	JS_SaveFrameChain(c);
}

inline void
restore_frame_chain(context &c)
{
	JS_RestoreFrameChain(c);
}

inline bool
report_exception(context &c)
{
	return JS_ReportPendingException(c);
}

inline bool
pending_exception(const context &c)
{
	return JS_IsExceptionPending(c);
}

inline JS::Zone *
current_zone()
{
	return current_zone(*cx);
}

inline JS::Zone *
current_zone(context &c)
{
	return ::js::GetContextZone(c);
}

inline JSObject *
current_global()
{
	return current_global(*cx);
}

inline JSObject *
current_global(context &c)
{
	return JS::CurrentGlobalOrNull(c);
}

inline JSCompartment *
current_compartment()
{
	return current_compartment(*cx);
}

inline JSCompartment *
current_compartment(context &c)
{
	return ::js::GetContextCompartment(c);
}

inline void
priv(context &c,
     privdata *const &ptr)
{
	// Free any existing object to overwrite/null
	delete priv(c);
	JS_SetSecondContextPrivate(c, ptr);
}

template<class T>
T *
priv(context &c)
{
	return dynamic_cast<T *>(static_cast<privdata *>(JS_GetSecondContextPrivate(c)));
}

template<class T>
const T *
priv(const context &c)
{
	return dynamic_cast<const T *>(static_cast<const privdata *>(JS_GetSecondContextPrivate(c)));
}

inline context &
our(JSContext *const &c)
{
	return *static_cast<context *>(JS_GetContextPrivate(c));
}

inline const context &
our(const JSContext *const &c)
{
	return *static_cast<const context *>(JS_GetContextPrivate(const_cast<JSContext *>(c)));
}

} // namespace js
} // namespace ircd

static_assert(sizeof(struct ircd::js::context::state) == 8, "");
