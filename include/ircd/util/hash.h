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
#define HAVE_IRCD_UTIL_HASH_H

// constexpr bernstein string hasher suite; these functions will hash the
// string at compile time leaving an integer residue at runtime. Decent
// primes are at least 7681 and 5381.

namespace ircd {
inline namespace util
{
	template<size_t PRIME = 7681>
	constexpr size_t hash(const string_view str, size_t i = 0, size_t r = PRIME) noexcept;

	template<size_t PRIME = 7681>
	constexpr size_t hash(const char16_t *const str, size_t i = 0, size_t r = PRIME) noexcept;

	// Note that at runtime this hash uses multiplication on every character
	// which can consume many cycles...
	template<size_t PRIME = 7681>
	size_t hash(const std::u16string &str, size_t i = 0, size_t r = PRIME) noexcept;
}}

/// Runtime hashing of a std::u16string (for js). Non-cryptographic.
template<size_t PRIME>
[[gnu::pure]]
inline size_t
ircd::util::hash(const std::u16string &str,
                 size_t i,
                 size_t r)
noexcept
{
	for(; i < str.size(); ++i)
		r = str[i] ^ (r * 33ULL);

	return r;
}

/// Runtime hashing of a string_view. Non-cryptographic.
template<size_t PRIME>
[[gnu::pure]]
constexpr size_t
ircd::util::hash(const string_view str,
                 size_t i,
                 size_t r)
noexcept
{
	for(; i < str.size(); ++i)
		r = str[i] ^ (r * 33ULL);

	return r;
}

/// Compile-time hashing of a wider string literal (for js). Non-cryptographic.
template<size_t PRIME>
[[gnu::pure]]
constexpr size_t
ircd::util::hash(const char16_t *const str,
                 size_t i,
                 size_t r)
noexcept
{
	for(assert(str); str[i]; ++i)
		r = str[i] ^ (r * 33ULL);

	return r;
}
