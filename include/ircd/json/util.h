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

// This section contains utilities which are useful other parts of ircd::json
// or developers directly. This interface itself (though not its definition)
// has little dependence on other headers in ircd::json. In contrast, a suite
// of rich developer tools which depend on various assets in ircd::json are
// found in tool.h instead.
namespace ircd::json
{
	template<size_t SIZE> struct buffer;

	using name_hash_t = size_t;
	constexpr name_hash_t name_hash(const string_view name) noexcept;
	constexpr name_hash_t operator ""_(const char *const name, const size_t len) noexcept;

	void valid_output(const string_view &, const size_t &expected);
	void debug_valid_output(const string_view &, const size_t &expected);

	size_t serialized(const string_view &);
	string_view stringify(mutable_buffer &, const string_view &);
	template<class... T> size_t print(const mutable_buffer &buf, T&&... t);

	// Validate JSON - checks if valid JSON (not canonical).
	bool valid(const string_view &, std::nothrow_t) noexcept;
	void valid(const string_view &);
	std::string why(const string_view &);

	struct stats extern stats;
}

/// Statistics counter access; unfortunately these cannot participate as
/// ircd::stats items right now.
struct ircd::json::stats
{
	uint64_t print_calls {0}, print_cycles {0};
	uint64_t parse_calls {0}, parse_cycles {0};
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
	debug_valid_output(sv, size(sv)); // no size expectation check
	return sv.size();
}

extern inline void
__attribute__((always_inline, gnu_inline, artificial))
ircd::json::debug_valid_output(const string_view &in,
                               const size_t &expected)
{
	#ifdef RB_DEBUG
	valid_output(in, expected);
	#endif
}

constexpr ircd::json::name_hash_t
ircd::json::operator ""_(const char *const text, const size_t len)
noexcept
{
	return name_hash(string_view(text, len));
}

constexpr ircd::json::name_hash_t
ircd::json::name_hash(const string_view name)
noexcept
{
	return ircd::hash(name);
}
