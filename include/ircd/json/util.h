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
#define HAVE_IRCD_JSON_UTIL_H

namespace ircd::json
{
	struct string;
	template<size_t SIZE> struct buffer;
	using name_hash_t = size_t;
	using members = std::initializer_list<member>;

	extern const string_view literal_null;
	extern const string_view literal_true;
	extern const string_view literal_false;
	extern const string_view empty_string;
	extern const string_view empty_object;
	extern const string_view empty_array;
	extern const int64_t undefined_number;

	constexpr name_hash_t name_hash(const string_view name);
	constexpr name_hash_t operator ""_(const char *const name, const size_t len);

	size_t serialized(const string_view &);
	string_view stringify(mutable_buffer &, const string_view &);
	template<class... T> size_t print(const mutable_buffer &buf, T&&... t);

	// Validate JSON - checks if valid JSON (not canonical).
	bool valid(const string_view &, std::nothrow_t) noexcept;
	void valid(const string_view &);
	std::string why(const string_view &);

	// (Internal) validates output
	void valid_output(const string_view &, const size_t &expected);

	// Transforms input into escaped output only
	string_view escape(const mutable_buffer &out, const string_view &in);
}

/// Strong type representing quoted strings in JSON (which may be unquoted
/// automatically when this type is encountered in a tuple etc)
struct ircd::json::string
:string_view
{
	string() = default;
	string(json::string &&) = default;
	string(const json::string &) = default;
	string(const string_view &s)
	:string_view
	{
		surrounds(s, '"')?
			unquote(s):
			s
	}{}

	string &operator=(json::string &&) = default;
	string &operator=(const json::string &) = default;
	string &operator=(const string_view &s)
	{
		*static_cast<string_view *>(this) = surrounds(s, '"')?
			unquote(s):
			s;

		return *this;
	}
};

/// Alternative to `json::strung` which uses a fixed array rather than an
/// allocated string as the target.
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

constexpr ircd::json::name_hash_t
ircd::json::operator ""_(const char *const text, const size_t len)
{
	return name_hash(string_view(text, len));
}

constexpr ircd::json::name_hash_t
ircd::json::name_hash(const string_view name)
{
	return ircd::hash(name);
}
