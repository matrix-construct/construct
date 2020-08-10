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
#define HAVE_IRCD_B64_H

namespace ircd::b64
{
	extern const u8
	dict_rfc1421[64],     // [62] = '+', [63] = '/'
	dict_rfc3501[64],     // [62] = '+', [63] = ','
	dict_rfc4648[64];     // [62] = '-', [63] = '_'

	// Binary -> Base64 conversion suite
	constexpr size_t encode_size(const size_t &) noexcept;
	size_t encode_size(const const_buffer &in) noexcept;
	string_view encode(const mutable_buffer &out, const const_buffer &in) noexcept;

	// Binary -> Base64 conversion without padding
	constexpr size_t encode_unpadded_size(const size_t &) noexcept;
	size_t encode_unpadded_size(const const_buffer &in) noexcept;
	string_view encode_unpadded(const mutable_buffer &out, const const_buffer &in) noexcept;

	// Base64 -> Binary conversion (padded or unpadded)
	constexpr size_t decode_size(const size_t &) noexcept;
	size_t decode_size(const string_view &in) noexcept;
	const_buffer decode(const mutable_buffer &out, const string_view &in);

	// Base64 convenience conversions
	string_view tob64url(const mutable_buffer &out, const string_view &in) noexcept;
	string_view urltob64(const mutable_buffer &out, const string_view &in) noexcept;
}

inline size_t
ircd::b64::decode_size(const string_view &in)
noexcept
{
	return decode_size(size(in));
}

constexpr size_t
ircd::b64::decode_size(const size_t &in)
noexcept
{
	return (in * 0.75) + 1; //XXX: constexpr ceil()
}

inline size_t
ircd::b64::encode_unpadded_size(const const_buffer &in)
noexcept
{
	return encode_unpadded_size(size(in));
}

constexpr size_t
ircd::b64::encode_unpadded_size(const size_t &in)
noexcept
{
	return (in * (4.0 / 3.0)) + 1; //XXX: constexpr ceil()
}

inline size_t
ircd::b64::encode_size(const const_buffer &in)
noexcept
{
	return encode_size(size(in));
}

constexpr size_t
ircd::b64::encode_size(const size_t &in)
noexcept
{
	return ((in * (4.0 / 3.0)) + 1) + (3 - in % 3) % 3; //XXX: constexpr ceil
}
