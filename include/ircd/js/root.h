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
#define HAVE_IRCD_JS_ROOT_H

namespace ircd  {
namespace js    {

enum class lifetime
{
	stack,       // entity lives on the stack and the GC uses a cheap forward list between objects
	heap,        // entity lives on the heap with dynamic duration
	tenured,     // long-life-optimized heap entity with special rules (must read jsapi docs)
	persist,     // entity has duration similar to the runtime itself
	maybe,       // template with a boolean switch for GC participation
	fake,        // noop; does not register with GC at all
};

template<class T,
         lifetime L>
struct root
{
};

template<class T>
using handle = typename T::handle;

template<class T>
struct root<T, lifetime::stack>
:JS::Rooted<T>
{
	using base_type = JS::Rooted<T>;
	using type = root<T, lifetime::stack>;
	using handle_mutable = JS::MutableHandle<T>;
	using handle = JS::Handle<T>;

	root(const handle &h)
	:JS::Rooted<T>{*cx, h}
	{
	}

	root(const handle_mutable &h)
	:JS::Rooted<T>{*cx, h}
	{
	}

	template<lifetime L>
	root(const root<T, L> &other)
	:JS::Rooted<T>{*cx, other.get()}
	{
	}

	explicit root(const T &t)
	:JS::Rooted<T>{*cx, t}
	{
	}

	root()
	:JS::Rooted<T>{*cx}
	{
	}

	root(root&& other)
	noexcept
	:JS::Rooted<T>{*cx, other.get()}
	{
	}

	root &operator=(root &&other)
	noexcept
	{
		this->set(other.get());
		return *this;
	}

	root(const root &) = delete;
	root &operator=(const root &) = delete;
};

// This conversion is missing in the jsapi. Is this object not gc rooted? Can it not have a handle?
template<class T>
JS::Handle<T>
operator&(const JS::Heap<T> &h)
{
	return JS::Handle<T>::fromMarkedLocation(h.address());
}

// This conversion is missing in the jsapi. Is this object not gc rooted? Can it not have a handle?
template<class T>
JS::MutableHandle<T>
operator&(JS::Heap<T> &h)
{
	const auto ptr(const_cast<T *>(h.address()));
	return JS::MutableHandle<T>::fromMarkedLocation(ptr);
}

template<class T>
struct root<T, lifetime::heap>
:JS::Heap<T>
{
	using type = root<T, lifetime::heap>;
	using handle = JS::Handle<T>;
	using handle_mutable = JS::MutableHandle<T>;

	operator handle() const
	{
		return JS::Handle<T>::fromMarkedLocation(this->address());
	}

	operator handle_mutable()
	{
		const auto ptr(const_cast<T *>(this->address()));
		return JS::MutableHandle<T>::fromMarkedLocation(ptr);
	}

	root(const handle &h)
	:JS::Heap<T>{h}
	{
	}

	root(const handle_mutable &h)
	:JS::Heap<T>{h}
	{
	}

	template<lifetime L>
	root(const root<T, L> &other)
	:JS::Heap<T>{other.get()}
	{
	}

	explicit root(const T &t)
	:JS::Heap<T>{t}
	{
	}

	root()
	:JS::Heap<T>{}
	{
	}

	root(root&&) = default;
	root(const root &) = default;
	root &operator=(root &&) = default;
	root &operator=(const root &) = default;
};

// This conversion is missing in the jsapi. Is this object not gc rooted? Can it not have a handle?
template<class T>
JS::Handle<T>
operator&(const JS::TenuredHeap<T> &h)
{
	return JS::Handle<T>::fromMarkedLocation(h.address());
}

// This conversion is missing in the jsapi. Is this object not gc rooted? Can it not have a handle?
template<class T>
JS::MutableHandle<T>
operator&(JS::TenuredHeap<T> &h)
{
	const auto ptr(const_cast<T *>(h.address()));
	return JS::MutableHandle<T>::fromMarkedLocation(ptr);
}

template<class T>
struct root<T, lifetime::tenured>
:JS::TenuredHeap<T>
{
	using type = root<T, lifetime::tenured>;
	using handle_mutable = JS::MutableHandle<T>;
	using handle = JS::Handle<T>;

	operator handle() const
	{
		return JS::Handle<T>::fromMarkedLocation(this->address());
	}

	operator handle_mutable()
	{
		const auto ptr(const_cast<T *>(this->address()));
		return JS::MutableHandle<T>::fromMarkedLocation(ptr);
	}

	auto operator&() const
	{
		return static_cast<handle>(*this);
	}

	auto operator&()
	{
		return static_cast<handle_mutable>(*this);
	}

	root(const handle &h)
	:JS::TenuredHeap<T>{h}
	{
	}

	root(const handle_mutable &h)
	:JS::TenuredHeap<T>{h}
	{
	}

	template<lifetime L>
	root(const root<T, L> &other)
	:JS::TenuredHeap<T>{other.get()}
	{
	}

	explicit root(const T &t)
	:JS::TenuredHeap<T>{t}
	{
	}

	root()
	:JS::TenuredHeap<T>{}
	{
	}

	root(root&&) = default;
	root(const root &) = default;
	root &operator=(root &&) = default;
	root &operator=(const root &) = default;
};

template<class T>
struct root<T, lifetime::persist>
:JS::PersistentRooted<T>
{
	using type = root<T, lifetime::persist>;
	using handle_mutable = JS::MutableHandle<T>;
	using handle = JS::Handle<T>;

	template<lifetime L>
	root(const root<T, L> &other)
	:JS::PersistentRooted<T>{*cx, other.get()}
	{
	}

	explicit root(const T &t)
	:JS::PersistentRooted<T>{*cx, t}
	{
	}

	root()
	:JS::PersistentRooted<T>{*cx}
	{
	}

	root(root&& other)
	noexcept
	:JS::PersistentRooted<T>{*cx, other.get()}
	{
	}

	root(const root &) = delete;
	root &operator=(const root &) = delete;
};

template<class T>
struct root<T, lifetime::maybe>
:public ::js::MaybeRooted<T, ::js::CanGC>
{
	using type = root<T, lifetime::maybe>;
};

template<class T>
struct root<T, lifetime::fake>
:public ::js::FakeRooted<T>
{
	using type = root<T, lifetime::fake>;
};

} // namespace js
} // namespace ircd
