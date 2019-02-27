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
#define HAVE_IRCD_JSON_STRING_H

namespace ircd::json
{
	struct string;
}

/// Strong type representing quoted strings in JSON (which may be unquoted
/// automatically when this type is encountered in a tuple etc).
struct ircd::json::string
:string_view
{
	string() = default;
	string(json::string &&) = default;
	string(const json::string &) = default;
	string(const string_view &s);

	string &operator=(json::string &&) = default;
	string &operator=(const json::string &) = default;
	string &operator=(const string_view &s);
};

inline
ircd::json::string::string(const string_view &s)
:string_view
{
	surrounds(s, '"')?
		unquote(s):
		s
}
{}

inline ircd::json::string &
ircd::json::string::operator=(const string_view &s)
{
	*static_cast<string_view *>(this) = surrounds(s, '"')?
		unquote(s):
		s;

	return *this;
}
