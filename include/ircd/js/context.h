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
#define HAVE_IRCD_JS_CONTEXT_H

namespace ircd {
namespace js   {

// Indicates the phase of execution of the javascript
enum class phase
:uint8_t
{
	LEAVE,         // JS is not running.
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

class context
:custom_ptr<JSContext>
{
	// JS engine callback surface.
	static void handle_error(JSContext *, JSErrorReport *) noexcept;
	static bool handle_interrupt(JSContext *) noexcept;
	static void handle_timeout(JSContext *) noexcept;
	static void handle_telemetry(int id, uint32_t sample, const char *key) noexcept;
	static bool handle_get_performance_groups(JSContext *, ::js::PerformanceGroupVector &, void *) noexcept;
	static bool handle_stopwatch_commit(uint64_t, ::js::PerformanceGroupVector &, void *) noexcept;
	static bool handle_stopwatch_start(uint64_t, void *) noexcept;
	static void handle_out_of_memory(JSContext *, void *) noexcept;
	static void handle_large_allocation_failure(void *) noexcept;
	static void handle_trace_gray(JSTracer *, void *) noexcept;
	static void handle_trace_extra(JSTracer *, void *) noexcept;
	static void handle_weak_pointer_zone(JSContext *, void *) noexcept;
	static void handle_weak_pointer_compartment(JSContext *, JSCompartment *, void *) noexcept;
	static void handle_zone_sweep(JS::Zone *) noexcept;
	static void handle_zone_destroy(JS::Zone *) noexcept;
	static void handle_compartment_name(JSContext *, JSCompartment *, char *buf, size_t) noexcept;
	static void handle_compartment_destroy(JSFreeOp *, JSCompartment *) noexcept;
	static void handle_finalize(JSFreeOp *, JSFinalizeStatus, bool is_compartment, void *) noexcept;
	static void handle_objects_tenured(JSContext *, void *) noexcept;
	static void handle_slice(JSContext *, JS::GCProgress, const JS::GCDescription &) noexcept;
	static void handle_gc(JSContext *, JSGCStatus, void *) noexcept;
	static bool handle_preserve_wrapper(JSContext *, JSObject *) noexcept;
	static void handle_activity(void *priv, bool active) noexcept;
	static bool handle_promise_enqueue_job(JSContext *, JS::HandleObject, JS::HandleObject, JS::HandleObject, void *) noexcept;
	static void handle_promise_rejection_tracker(JSContext *, JS::HandleObject, PromiseRejectionHandlingState, void *) noexcept;
	static bool handle_start_async_task(JSContext *, JS::AsyncTask *) noexcept;
	static bool handle_finish_async_task(JS::AsyncTask *) noexcept;
	static JSObject *handle_get_incumbent_global(JSContext *) noexcept;
	static bool handle_set_build_id_op(JS::BuildIdCharVector *) noexcept;

  public:
	struct opts;
	struct alignas(8) state
	{
		uint32_t sem;
		enum phase phase;
		enum irq irq;
	};

	std::unique_ptr<struct opts> opts;           // Options for the context.
	std::thread::id tid;                         // This is recorded for assertions/logging.
	struct tracing tracing;                      // State for garbage collection / tracing.
	JSExceptionState *except;                    // Use save_exception()/restore_exception()
	std::atomic<struct state> state;             // Atomic state of execution
	std::function<int (const irq &)> on_intr;    // User interrupt hook (ret -1 to not interfere)
	struct timer timer;                          // Preemption timer
	struct star *star;                           // System target

	// JSContext
	operator JSContext *() const                 { return get();                                   }
	operator JSContext &() const                 { return custom_ptr<JSContext>::operator*();      }
	bool operator!() const                       { return !custom_ptr<JSContext>::operator bool(); }
	auto ptr() const                             { return get();                                   }
	auto ptr()                                   { return get();                                   }

	// BasicLockable                             // std::lock_guard<context>
	void lock()                                  { JS_BeginRequest(get());                         }
	void unlock()                                { JS_EndRequest(get());                           }

	context(std::unique_ptr<struct opts> opts, JSContext *const &parent = nullptr);
	context(const struct opts &, JSContext *const &parent = nullptr);
	context() = default;
	context(context &&) = delete;
	context(const context &) = delete;
	~context() noexcept;
};

// Options for the context. Most of these values will never change from what
// the user initially specified, but this is not held as const by the context to
// allow for JS code itself to change its own options if possible.
struct context::opts
{
	size_t max_bytes              = 64_MiB;
	size_t max_nursery_bytes      = 16_MiB;
	size_t code_stack_max         = 0;
	size_t trusted_stack_max      = 0;
	size_t untrusted_stack_max    = 0;
	size_t stack_chunk_size       = 8_KiB;
	microseconds timer_limit      = 10s;              //TODO: Temp
	bool concurrent_parsing       = true;
	bool concurrent_jit           = true;
	uint8_t gc_zeal_mode          = 0;                     // normal
	uint32_t gc_zeal_freq         = JS_DEFAULT_ZEAL_FREQ;  // 100
};

// Current thread_local context. This value affects contextual data for almost every function
// in this entire subsystem (ircd::js). Located in ircd/js.cc.
extern __thread context *cx;

// Get to our own `struct context` from any upstream JSContext*
const context &our(const JSContext *const &);
context &our(JSContext *const &);

// Misc
inline auto running(const context &c)            { return JS_IsRunning(c);                         }
inline auto version(const context &c)            { return version(JS_GetVersion(c));               }

// Current
JS::Zone *current_zone(context & = *cx);
JSObject *current_global(context & = *cx);
JSCompartment *current_compartment(context & = *cx);

// Memory
void set(context &c, const JSGCParamKey &, const uint32_t &val);
uint32_t get(context &c, const JSGCParamKey &);
void out_of_memory(context &c);
void allocation_overflow(context &c);
bool maybe_gc(context &c) noexcept;
bool run_gc(context &c) noexcept;

// Exception
bool pending_exception(const context &c);
void clear_exception(context &c);
void save_exception(context &c);
bool restore_exception(context &c);

// Interruption
bool interrupt(context &, const irq &);
bool interrupt_poll(const context &c);

// Execution
void enter(context &);  // throws if can't enter
void leave(context &);  // must be called if enter() succeeds

// (Convenience) enter JS within this closure. Most likely your function
// will return a `struct value` or JS::Value returned by most calls into JS.
template<class F> auto run(F&& function);


template<class F>
auto
run(F&& function)
{
	assert(!pending_exception(*cx));

	enter(*cx);
	const unwind out([]
	{
		leave(*cx);
	});

	return function();
}

inline void
clear_exception(context &c)
{
	return JS_ClearPendingException(c);
}

inline bool
pending_exception(const context &c)
{
	return JS_IsExceptionPending(c);
}

inline JSObject *
current_global(context &c)
{
	return JS::CurrentGlobalOrNull(c);
}

inline JSCompartment *
current_compartment(context &c)
{
	return ::js::GetContextCompartment(c);
}

inline JS::Zone *
current_zone(context &c)
{
	return ::js::GetContextZone(c);
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
