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
	using members = std::initializer_list<const member>;

	/// strict_t overloads scan the whole string to determine both the type
	/// and validity of the string. For large strings this may involve a lot
	/// of memory operations; if the validity is known good it is better to
	/// not use the strict overload.
	IRCD_OVERLOAD(strict)

	// Determine the type
	enum type type(const string_view &);
	enum type type(const string_view &, std::nothrow_t);
	enum type type(const string_view &, strict_t);
	enum type type(const string_view &, strict_t, std::nothrow_t);

	// Query if type
	bool type(const string_view &, const enum type &, strict_t);
	bool type(const string_view &, const enum type &);

	// Utils
	string_view reflect(const enum type &);

	extern const string_view literal_null;
	extern const string_view literal_true;
	extern const string_view literal_false;
	extern const string_view empty_string;
	extern const string_view empty_object;
	extern const string_view empty_array;
	extern const int64_t undefined_number;
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
