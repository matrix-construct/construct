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
#define HAVE_IRCD_JS_ROOT_H

namespace ircd  {
namespace js    {

template<class T>
using handle = typename T::handle;

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
struct root
:JS::Heap<T>
{
	using value_type = T;
	using base_type = JS::Heap<T>;
	using handle = JS::Handle<T>;
	using handle_mutable = JS::MutableHandle<T>;
	using type = root<value_type>;

	operator handle() const
	{
		const auto ptr(this->address());
		return JS::Handle<T>::fromMarkedLocation(ptr);
	}

	operator handle_mutable()
	{
		const auto ptr(this->address());
		return JS::MutableHandle<T>::fromMarkedLocation(ptr);
	}

  private:
	tracing::list::iterator tracing_it;

	tracing::list::iterator add(base_type *const &ptr)
	{
		return cx->tracing.heap.emplace(end(cx->tracing.heap), tracing::thing { ptr, js::type<T>() });
	}

	void del(const tracing::list::iterator &tracing_it)
	{
		if(tracing_it != end(cx->tracing.heap))
		{
			const void *ptr = tracing_it->ptr;
			cx->tracing.heap.erase(tracing_it);
		}
	}

  public:
	root(const handle &h)
	:base_type{h.get()}
	,tracing_it{add(this)}
	{
	}

	root(const handle_mutable &h)
	:base_type{h.get()}
	,tracing_it{add(this)}
	{
	}

	explicit root(const T &t)
	:base_type{t}
	,tracing_it{add(this)}
	{
	}

	root()
	:base_type{}
	,tracing_it{add(this)}
	{
	}

	root(root &&other) noexcept
	:base_type{other.get()}
	,tracing_it{std::move(other.tracing_it)}
	{
		other.tracing_it = end(cx->tracing.heap);
		tracing_it->ptr = static_cast<base_type *>(this);
	}

	root(const root &other)
	:base_type{other.get()}
	,tracing_it{add(this)}
	{
	}

	root &operator=(root &&other) noexcept
	{
		base_type::operator=(std::move(other.get()));
		return *this;
	}

	root &operator=(const root &other)
	{
		base_type::operator=(other.get());
		return *this;
	}

	~root() noexcept
	{
		del(tracing_it);
	}
};

} // namespace js
} // namespace ircd
