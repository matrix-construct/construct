// The Construct
//
// Copyright (C) The Construct Developers, Authors & Contributors
// Copyright (C) 2016-2020 Jason Volk <jason@zemos.net>
//
// Permission to use, copy, modify, and/or distribute this software for any
// purpose with or without fee is hereby granted, provided that the above
// copyright notice and this permission notice is present in all copies. The
// full license for this software is available in the LICENSE file.

#pragma once
#define HAVE_IRCD_B58_H

namespace ircd::b58
{
	// Binary -> Base58 encode suite
	constexpr size_t encode_size(const size_t &) noexcept;
	size_t encode_size(const const_buffer &in) noexcept;
	string_view encode(const mutable_buffer &out, const const_buffer &in) noexcept;

	// Base58 -> Binary decode suite
	constexpr size_t decode_size(const size_t &) noexcept;
	size_t decode_size(const string_view &in) noexcept;
	const_buffer decode(const mutable_buffer &out, const string_view &in);

	// Convenience conversions
	string_view tob64(const mutable_buffer &out, const string_view &in);
	string_view tob64_unpadded(const mutable_buffer &out, const string_view &in);
	string_view fromb64(const mutable_buffer &out, const string_view &in);
}

inline size_t
ircd::b58::decode_size(const string_view &in)
noexcept
{
	return decode_size(size(in));
}

constexpr size_t
ircd::b58::decode_size(const size_t &in)
noexcept
{
	return in * 733UL / 1000UL + 1UL; // log(58) / log(256), rounded up
}

inline size_t
ircd::b58::encode_size(const const_buffer &in)
noexcept
{
	return encode_size(size(in));
}

constexpr size_t
ircd::b58::encode_size(const size_t &in)
noexcept
{
	return in * 138UL / 100UL + 1UL; // log(256) / log(58), rounded up
}
