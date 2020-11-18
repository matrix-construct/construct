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

	// note: in is not a json::string; all characters viewed are candidate
	string escape(const mutable_buffer &out, const string_view &in);

	// note: in is a json::string and return is a const_buffer to force
	// explicit conversions because this operation generates binary data.
	const_buffer unescape(const mutable_buffer &out, const string &in);
}

/// Strong type representing quoted strings in JSON (which may be unquoted
/// automatically when this type is encountered in a tuple etc).
struct ircd::json::string
:string_view
{
	// Note that the input argument is not a json::string; the caller must
	// strip surrounding quotes from the view otherwise they will be counted
	// with their missing escapes in the return value. This is by design to
	// avoid unintentionally stripping quotes from actual payloads.
	static size_t serialized(const string_view &in) noexcept;

	// Transform input into canonical string content. The output buffer must
	// be at least the size reported by serialized() on the same input.
	static size_t stringify(const mutable_buffer &out, const string_view &in) noexcept;

	string() = default;
	string(json::string &&) = default;
	string(const json::string &) = default;
	string(const string_view &s) noexcept;

	string &operator=(json::string &&) = default;
	string &operator=(const json::string &) = default;
	string &operator=(const string_view &s) noexcept;
};

inline
ircd::json::string::string(const string_view &s)
noexcept
:string_view
{
	surrounds(s, '"')?
		s.substr(1, s.size() - 2):
		std::string_view(s)
}
{}

inline ircd::json::string &
ircd::json::string::operator=(const string_view &s)
noexcept
{
	*static_cast<string_view *>(this) = surrounds(s, '"')?
		s.substr(1, s.size() - 2):
		std::string_view(s);

	return *this;
}
