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
		using JS::HandleValueArray::HandleValueArray;
		handle(): JS::HandleValueArray{JS::HandleValueArray::empty()} {}
	};

	// Construct vector from initializer list of raw `JS::Value`
	// ex: JS::Value a; vector foo {{ a, a, ... }};
	vector(const std::initializer_list<jsapi_type> &list)
	:JS::AutoVectorRooter<jsapi_type>{*cx}
	{
		reserve(list.size());
		for(auto &t : list)
			infallibleAppend(t);
	}

	// Construct from initializer list of our `struct value` wrapper
	// ex: value a(1); vector foo {{ a, a, ... }};
	vector(const std::initializer_list<local_type> &list)
	:JS::AutoVectorRooter<jsapi_type>{*cx}
	{
		reserve(list.size());
		for(auto &t : list)
			infallibleAppend(t.get());
	}

	// Construct from initializer list of any type passed through `struct value` ctor
	// ex: int a; vector foo {{ a, 3, 123, ... }};
	template<class U>
	vector(const std::initializer_list<U> &list)
	:JS::AutoVectorRooter<jsapi_type>{*cx}
	{
		reserve(list.size());
		for(auto &t : list)
			infallibleAppend(value(t));
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

} // namespace js
} // namespace ircd
