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

struct context
:private custom_ptr<JSContext>
{
	class lock
	{
		context *c;

	  public:
		lock(context &c);           // BeginRequest on cx of your choice
		lock();                     // BeginRequest on the thread_local cx
		~lock() noexcept;           // EndRequest
	};

	struct opts
	{
		size_t stack_chunk_size     = 8_KiB;
		bool dtor_gc                = true;
	}
	opts;                                        // We keep a copy of the given opts here.

	operator JSContext *() const                 { return get();                                   }
	operator JSContext &() const                 { return custom_ptr<JSContext>::operator*();      }
	bool operator!() const                       { return !custom_ptr<JSContext>::operator bool(); }
	auto &runtime() const                        { return our(JS_GetRuntime(get()));               }
	auto &runtime()                              { return our(JS_GetRuntime(get()));               }
	auto ptr() const                             { return get();                                   }
	auto ptr()                                   { return get();                                   }

	context(JSRuntime *const &, const struct opts &);
	context() = default;
	context(context &&) noexcept;
	context(const context &) = delete;
	context &operator=(context &&) noexcept;
	context &operator=(const context &) = delete;
	~context() noexcept;
};

// Current thread_local context. Runtimes/Contexts (soon to be merged in future SpiderMonkey)
// are singled-threaded and this points to the context appropos your thread.
// Do not construct more than one context on the same thread- this is overwritten.
extern __thread context *cx;

// A default JSContext instance is provided residing near the main runtime as a convenience
// for misc/utility/system purposes if necessary. You should use *cx instead.
extern context main_cx;

// Get to our `struct context` from any upstream JSContext
const context &our(const JSContext *const &);
context &our(JSContext *const &);

// Get/Set your privdata managed by this object, casting to your expected type.
template<class T = privdata> const T *priv(const context &);
template<class T = privdata> T *priv(context &);
void priv(context &, privdata *const &);

inline auto version(const context &c)            { return version(JS_GetVersion(c));               }
inline auto running(const context &c)            { return JS_IsRunning(c);                         }
inline auto uncaught_exception(const context &c) { return JS_IsExceptionPending(c);                }
inline auto rethrow_exception(context &c)        { return JS_ReportPendingException(c);            }
inline auto interrupted(const context &c)        { return JS_CheckForInterrupt(c);                 }
inline void out_of_memory(context &c)            { JS_ReportOutOfMemory(c);                        }
inline void allocation_overflow(context &c)      { JS_ReportAllocationOverflow(c);                 }
inline void run_gc(context &c)                   { JS_MaybeGC(c);                                  }
JSObject *current_global(context &c);
JSObject *current_global();                      // thread_local


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

inline void
priv(context &c,
     privdata *const &ptr)
{
	delete priv(c);                          // Free any existing object to overwrite/null
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
