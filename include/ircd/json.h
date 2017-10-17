/*
 * Copyright (C) 2017 Charybdis Development Team
 * Copyright (C) 2017 Jason Volk <jason@zemos.net>
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
#define HAVE_IRCD_JSON_H

/// JavaScript Object Notation: formal grammars & tools
///
namespace ircd::json
{
	IRCD_EXCEPTION(ircd::error, error);
	IRCD_EXCEPTION(error, parse_error);
	IRCD_EXCEPTION(error, print_error);
	IRCD_EXCEPTION(error, type_error);
	IRCD_EXCEPTION(error, not_found);

	struct value;
	struct member;
	struct object;
	struct array;
	struct iov;

	enum type
	{
		STRING  = 0,
		OBJECT  = 1,
		ARRAY   = 2,
		NUMBER  = 3,
		LITERAL = 4,
	};
	enum type type(const string_view &);
	enum type type(const string_view &, std::nothrow_t);
	string_view reflect(const enum type &);

	using name_hash_t = size_t;
	constexpr name_hash_t name_hash(const char *const name, const size_t len = 0);
	constexpr name_hash_t name_hash(const string_view &name);
	constexpr name_hash_t operator ""_(const char *const name, const size_t len);

	/// Higher order type beyond a string to cleanly delimit multiple keys.
	using path = std::initializer_list<string_view>;
	std::ostream &operator<<(std::ostream &, const path &);

	/// These templates are generic frontends for building a JSON string. They
	/// eventually all lead to the stringify() friend function of the argument
	/// you pass to the template.
	template<class... T> string_view stringify(const mutable_buffer &&mb, T&&... t);
	template<class... T> size_t print(char *const &buf, const size_t &max, T&&... t);
	template<class... T> size_t print(const mutable_buffer &buf, T&&... t);
	template<size_t SIZE> struct buffer;
	struct strung;

	size_t serialized(const string_view &);

	struct string;
	using members = std::initializer_list<member>;
}

/// Strong type representing quoted strings in JSON (which may be unquoted
/// automatically when this type is encountered in a tuple etc)
struct ircd::json::string
:string_view
{
	using string_view::string_view;
};

#include "json/array.h"
#include "json/object.h"

/// Convenience template to allocate std::string and print() arguments to it.
///
struct ircd::json::strung
:std::string
{
	template<class... T> strung(T&&... t);
};

#include "json/value.h"
#include "json/member.h"
#include "json/property.h"
#include "json/iov.h"
#include "json/tuple.h"

namespace ircd
{
	using json::operator ""_;
	using json::operator<<;
	using json::defined;
	using json::for_each;
	using json::until;
}

template<size_t SIZE>
struct ircd::json::buffer
:string_view
{
	std::array<char, SIZE> b;

	template<class... T>
	buffer(T&&... t)
	:string_view{stringify(b, std::forward<T>(t)...)}
	{}
};

///
/// Convenience template for const rvalue mutable_buffers or basically
/// allowing a bracket initialization of a mutable_buffer in the argument
/// to stringify()
///
template<class... T>
ircd::string_view
ircd::json::stringify(const mutable_buffer &&mb,
                      T&&... t)
{
	mutable_buffer mbc{mb};
	return stringify(mbc, std::forward<T>(t)...);
}

///
/// Convenience template using the syntax print(mutable_buffer, ...)
/// which stringifies with null termination into buffer.
///
template<class... T>
size_t
ircd::json::print(const mutable_buffer &buf,
                  T&&... t)
{
	return print(data(buf), size(buf), std::forward<T>(t)...);
}

///
/// Convenience template using the syntax print(buf, sizeof(buf), ...)
/// which stringifies with null termination into buffer.
///
template<class... T>
size_t
ircd::json::print(char *const &buf,
                  const size_t &max,
                  T&&... t)
{
	if(unlikely(!max))
		return 0;

	mutable_buffer mb
	{
		buf, max - 1
	};

	const auto sv
	{
		stringify(mb, std::forward<T>(t)...)
	};

	assert(sv.size() < max);
	buf[sv.size()] = '\0';
	return sv.size();
}

template<class... T>
ircd::json::strung::strung(T&&... t)
:std::string{[&]() -> std::string
{
	const auto size
	{
		serialized(std::forward<T>(t)...)
	};

	std::string ret(size, char{});
	const auto buf{const_cast<char *>(ret.data())};
	const auto max{ret.size() + 1};
	const auto printed
	{
		print(buf, max, std::forward<T>(t)...)
	};

	#ifdef RB_DEBUG
	if(unlikely(printed != ret.size()))
		std::cerr << printed << " != " << ret.size() << std::endl << ret << std::endl;
	#endif

	assert(printed == ret.size());
	return ret;
}()}
{
}

inline std::ostream &
ircd::json::operator<<(std::ostream &s, const path &p)
{
	auto it(std::begin(p));
	if(it != std::end(p))
	{
		s << *it;
		++it;
	}

	for(; it != std::end(p); ++it)
		s << '.' << *it;

	return s;
}

constexpr ircd::json::name_hash_t
ircd::json::operator ""_(const char *const text, const size_t len)
{
	return name_hash(text, len);
}

constexpr ircd::json::name_hash_t
ircd::json::name_hash(const string_view &name)
{
	return ircd::hash(name);
}

constexpr ircd::json::name_hash_t
ircd::json::name_hash(const char *const name, const size_t len)
{
	return ircd::hash(name);
}
