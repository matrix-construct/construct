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
#define HAVE_IRCD_JSON_H

/// JavaScript Object Notation: formal grammars & tools
///
namespace ircd::json
{
	IRCD_EXCEPTION(ircd::error, error);
	IRCD_ASSERTION(error, print_error);
	IRCD_EXCEPTION(error, parse_error);
	IRCD_EXCEPTION(error, type_error);
	IRCD_EXCEPTION(error, not_found);
	IRCD_EXCEPTION(parse_error, recursion_limit);
	struct expectation_failure;

	struct value;
	struct member;
	struct string;
	struct object;
	struct array;
	struct vector;
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

	struct strung;
	template<size_t SIZE> struct buffer;
	template<class... T> string_view stringify(const mutable_buffer &&mb, T&&... t);
	template<class... T> size_t print(const mutable_buffer &buf, T&&... t);
}

/// Convenience template to allocate std::string and print() arguments to it.
struct ircd::json::strung
:std::string
{
	explicit operator json::object() const;
	explicit operator json::array() const;

	template<class... T> strung(T&&... t);
	strung(strung &&s) noexcept: std::string{std::move(s)} {}
	strung(const strung &s): std::string{s} {}
};

#include "util.h"
#include "array.h"
#include "object.h"
#include "vector.h"
#include "value.h"
#include "member.h"
#include "iov.h"
#include "tuple/tuple.h"
#include "stack.h"

namespace ircd
{
	using json::operator ""_;
	using json::operator<<;
	using json::defined;
	using json::for_each;
	using json::until;
	using json::get;
}

/// Strong type representing quoted strings in JSON (which may be unquoted
/// automatically when this type is encountered in a tuple etc)
struct ircd::json::string
:string_view
{
	string(const string_view &s)
	:string_view{unquote(s)}
	{}

	string() = default;
};

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

/// Convenience template for const rvalue mutable_buffers or basically
/// allowing a bracket initialization of a mutable_buffer in the argument
/// to stringify(). The const rvalue reference is on purpose. The stringify()
/// family of friends all use a non-const lvalue mutable_buffer as an append
/// only "stream" buffer. We don't want to modify any non-const instances of
/// the mutable_buffer you pass here by offering a `mutable_buffer &` overload
///
template<class... T>
ircd::string_view
ircd::json::stringify(const mutable_buffer &&mb,
                      T&&... t)
{
	mutable_buffer mbc{mb};
	return stringify(mbc, std::forward<T>(t)...);
}

/// Convenience template using the syntax print(mutable_buffer, ...)
/// which stringifies with null termination into buffer.
///
template<class... T>
size_t
ircd::json::print(const mutable_buffer &buf,
                  T&&... t)
{
	if(unlikely(!size(buf)))
		return 0;

	mutable_buffer out
	{
		data(buf), size(buf) - 1
	};

	const auto sv
	{
		stringify(out, std::forward<T>(t)...)
	};

	buf[sv.size()] = '\0';
	valid_output(sv, size(sv)); // no size expectation check
	return sv.size();
}

template<class... T>
ircd::json::strung::strung(T&&... t)
:std::string
{
	util::string(serialized(std::forward<T>(t)...), [&t...]
	(const mutable_buffer &out)
	{
		const auto sv
		{
			stringify(mutable_buffer{out}, std::forward<T>(t)...)
		};

		valid_output(sv, ircd::size(out));
		return sv;
	})
}{}

inline ircd::json::strung::operator
json::array()
const
{
	return string_view{*this};
}

inline ircd::json::strung::operator
json::object()
const
{
	return string_view{*this};
}
