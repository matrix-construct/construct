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
#define HAVE_IRCD_JS_VECTOR_H

namespace ircd {
namespace js   {

template<class T>
struct vector
:JS::AutoVectorRooter<T>
{
	using jsapi_type = T;
	using local_type = T;

	vector(const size_t &size)
	:JS::AutoVectorRooter<jsapi_type>{*cx}
	{
		this->resize(size);
	}

	vector()
	:JS::AutoVectorRooter<jsapi_type>{*cx}
	{
	}

	vector(vector &&other) noexcept
	:JS::AutoVectorRooter<jsapi_type>{*cx}
	{
		this->reserve(other.length());
		for(auto &t : other)
			this->infallibleAppend(t);

		other.clear();
	}
};

template<>
struct vector<value>
:JS::AutoVectorRooter<JS::Value>
{
	using jsapi_type = JS::Value;
	using local_type = value;

	struct handle
	:JS::HandleValueArray
	{
		handle()
		:JS::HandleValueArray{JS::HandleValueArray::empty()}
		{}

		handle(const JS::CallArgs &args)
		:JS::HandleValueArray{args}
		{}

		handle(const size_t &len, const JS::Value *const &elems)
		:JS::HandleValueArray{JS::HandleValueArray::fromMarkedLocation(len, elems)}
		{}
	};

	operator handle() const
	{
		return { length(), begin() };
	}

	/*
	// Construct vector from initializer list of raw `JS::Value`
	// ex: JS::Value a; vector foo {{ a, a, ... }};
	explicit vector(const std::initializer_list<jsapi_type> &list)
	:JS::AutoVectorRooter<jsapi_type>{*cx}
	{
		reserve(list.size());
		for(auto &t : list)
			infallibleAppend(t);
	}
	*/

	// Construct from initializer list of our `struct value` wrapper
	// ex: value a(1); vector foo {{ a, a, ... }};

	vector(const std::initializer_list<value> &list)
	:JS::AutoVectorRooter<jsapi_type>{*cx}
	{
		reserve(list.size());
		for(auto &t : list)
			infallibleAppend(t);
	}

	// Construct from initializer list of any type passed through `struct value` ctor
	// ex: int a; vector foo {{ a, 3, 123, ... }};
/*
	template<class U>
	vector(const std::initializer_list<U> &list)
	:JS::AutoVectorRooter<jsapi_type>{*cx}
	{
		reserve(list.size());
		for(auto &t : list)
			infallibleAppend(value(t));
	}
*/
	explicit vector(const object &obj)
	:JS::AutoVectorRooter<jsapi_type>{*cx}
	{
		if(!is_array(obj))
			throw internal_error("Object is not an array");

		const auto len(obj.size());
		reserve(obj.size());
		for(size_t i(0); i < len; ++i)
			infallibleAppend(get(obj, i));
	}

	vector(const value &val)
	:vector(object(val))
	{
	}

	vector(const handle &h)
	:JS::AutoVectorRooter<jsapi_type>{*cx}
	{
		reserve(h.length());
		for(size_t i(0); i < h.length(); ++i)
			infallibleAppend(h[i]);
	}

	vector(const size_t &size)
	:JS::AutoVectorRooter<jsapi_type>{*cx}
	{
		resize(size);
	}

	vector()
	:JS::AutoVectorRooter<jsapi_type>{*cx}
	{
	}

	vector(vector &&other) noexcept
	:JS::AutoVectorRooter<jsapi_type>{*cx}
	{
		reserve(other.length());
		for(auto &t : other)
			infallibleAppend(t);

		other.clear();
	}
};

// Specialization for vector of objects
template<>
struct vector<object>
:JS::AutoVectorRooter<JSObject *>
{
	using jsapi_type = JSObject *;
	using local_type = object;

	vector(const std::initializer_list<jsapi_type> &list)
	:JS::AutoVectorRooter<jsapi_type>{*cx}
	{
		reserve(list.size());
		for(auto &t : list)
			infallibleAppend(t);
	}

	vector(const std::initializer_list<local_type> &list)
	:JS::AutoVectorRooter<jsapi_type>{*cx}
	{
		reserve(list.size());
		for(auto &t : list)
			infallibleAppend(t.get());
	}

	vector(const size_t &size)
	:JS::AutoVectorRooter<jsapi_type>{*cx}
	{
		resize(size);
	}

	vector()
	:JS::AutoVectorRooter<jsapi_type>{*cx}
	{
	}
};

template<>
struct vector<id>
:JS::AutoVectorRooter<jsid>
{
	using jsapi_type = jsid;
	using local_type = id;
	using base_type = JS::AutoVectorRooter<jsapi_type>;
/*
	vector(const std::initializer_list<jsapi_type> &list)
	:JS::AutoVectorRooter<jsapi_type>{*cx}
	{
		reserve(list.size());
		for(auto &t : list)
			infallibleAppend(t);
	}
*/
	vector(const std::initializer_list<local_type> &list)
	:base_type{*cx}
	{
		reserve(list.size());
		for(auto &t : list)
			infallibleAppend(t.get());
	}

	vector(const size_t &size)
	:base_type{*cx}
	{
		resize(size);
	}

	vector()
	:base_type{*cx}
	{
		reserve(8);
	}
};

} // namespace js
} // namespace ircd
