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
:custom_ptr<JSContext>
{
	struct privdata
	{
		virtual ~privdata() noexcept = 0;
	};

	operator JSContext *() const                 { return get();                                   }
	operator JSContext &() const                 { return custom_ptr<JSContext>::operator*();      }

	void reset()                                 { custom_ptr<JSContext>::reset(nullptr);          }

	template<class... args> context(JSRuntime *const &, args&&...);
	context() = default;
};

// A default JSContext instance is provided residing near the main runtime as a convenience
// for misc/utility/system purposes if necessary.
extern context mc;

// Get/Set your privdata managed by this object, casting to your expected type.
template<size_t at, class T = context::privdata> const T *priv(const JSContext &);
template<size_t at, class T = context::privdata> T *priv(JSContext &);
template<size_t at, class T> void priv(JSContext &, T *);


template<class... args>
context::context(JSRuntime *const &runtime,
                 args&&... a)
:custom_ptr<JSContext>
{
	JS_NewContext(runtime, std::forward<args>(a)...),
	[](JSContext *const ctx)
	{
		if(likely(ctx))
		{
			delete priv<0>(*ctx);
			delete priv<1>(*ctx);
			JS_DestroyContext(ctx);
		}
	}
}
{
}

template<size_t at,
         class T>
void
priv(JSContext &c,
     T *const &ptr)
{
	delete priv<at>(c);
	switch(at)
	{
		default:
		case 0:   JS_SetContextPrivate(&c, ptr);            break;
		case 1:   JS_SetSecondContextPrivate(&c, ptr);      break;
	}
}

template<size_t at,
         class T>
T *
priv(JSContext &c)
{
	switch(at)
	{
		default:
		case 0:  return dynamic_cast<T *>(static_cast<context::privdata *>(JS_GetContextPrivate(&c)));
		case 1:  return dynamic_cast<T *>(static_cast<context::privdata *>(JS_GetSecondContextPrivate(&c)));
	}
}

template<size_t at,
         class T>
const T *
priv(const JSContext &c)
{
	switch(at)
	{
		default:
		case 0:  return dynamic_cast<const T *>(static_cast<const context::privdata *>(JS_GetContextPrivate(&c)));
		case 1:  return dynamic_cast<const T *>(static_cast<const context::privdata *>(JS_GetSecondContextPrivate(&c)));
	}
}

} // namespace js
} // namespace ircd
