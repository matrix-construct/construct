// Matrix Construct
//
// Copyright (C) Matrix Construct Developers, Authors & Contributors
// Copyright (C) 2016-2019 Jason Volk <jason@zemos.net>
//
// Permission to use, copy, modify, and/or distribute this software for any
// purpose with or without fee is hereby granted, provided that the above
// copyright notice and this permission notice is present in all copies. The
// full license for this software is available in the LICENSE file.

#pragma once
#define HAVE_IRCD_JSON_TYPE_H

namespace ircd::json
{
	enum type :uint8_t;
	struct value;
	struct member;
	struct string;
	struct object;
	struct array;
	struct vector;
	struct iov;
	using members = std::initializer_list<member>;

	/// strict_t overloads scan the whole string to determine both the type
	/// and validity of the string. For large strings this may involve a lot
	/// of memory operations; if the validity is known good it is better to
	/// not use the strict overload.
	IRCD_OVERLOAD(strict)

	// Utils
	[[gnu::pure]] string_view reflect(const enum type) noexcept;

	// Determine the type w/ strict correctness (full scan)
	[[gnu::pure]] bool type(const string_view &, const enum type, strict_t) noexcept;
	[[gnu::pure]] enum type type(const string_view &, strict_t, std::nothrow_t) noexcept;
	enum type type(const string_view &, strict_t);

	// Determine the type quickly
	[[gnu::pure]] bool type(const string_view &, const enum type) noexcept;
	[[gnu::pure]] enum type type(const string_view &, std::nothrow_t) noexcept;
	enum type type(const string_view &);
}

enum ircd::json::type
:uint8_t
{
	STRING  = 0,
	OBJECT  = 1,
	ARRAY   = 2,
	NUMBER  = 3,
	LITERAL = 4,
};
