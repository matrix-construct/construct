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
	using dictionary_element = int;
	using dictionary = [[aligned(64)]] dictionary_element[64];

	extern const dictionary
	dict_rfc1421,           // [62] = '+', [63] = '/'
	dict_rfc3501,           // [62] = '+', [63] = ','
	dict_rfc4648;           // [62] = '-', [63] = '_'

	static const auto
	&standard { dict_rfc1421 },
	&mailbox  { dict_rfc3501 },
	&urlsafe  { dict_rfc4648 };

	constexpr size_t decode_size(const size_t &) noexcept;
	constexpr size_t encode_size(const size_t &) noexcept;
	constexpr size_t encode_unpadded_size(const size_t &) noexcept;

	size_t decode_size(const string_view &in) noexcept;
	size_t encode_size(const const_buffer &in) noexcept;
	size_t encode_unpadded_size(const const_buffer &in) noexcept;

	const_buffer decode(const mutable_buffer &out, const string_view &in);

	template<const dictionary & = standard>
	string_view encode(const mutable_buffer &out, const const_buffer &in) noexcept;

	template<const dictionary & = standard>
	string_view encode_unpadded(const mutable_buffer &out, const const_buffer &in) noexcept;
}

inline size_t
ircd::b64::encode_unpadded_size(const const_buffer &in)
noexcept
{
	return encode_unpadded_size(size(in));
}

inline size_t
ircd::b64::encode_size(const const_buffer &in)
noexcept
{
	return encode_size(size(in));
}

inline size_t
ircd::b64::decode_size(const string_view &in)
noexcept
{
	const size_t pads
	{
		endswith_count(in, '=')
	};

	return decode_size(size(in) - pads);
}

constexpr size_t
ircd::b64::encode_unpadded_size(const size_t &in)
noexcept
{
	return (in * (4.0 / 3.0)) + 1; //XXX: constexpr ceil()
}

constexpr size_t
ircd::b64::encode_size(const size_t &in)
noexcept
{
	return ((in * (4.0 / 3.0)) + 1) + (3 - in % 3) % 3; //XXX: constexpr ceil
}

constexpr size_t
ircd::b64::decode_size(const size_t &in)
noexcept
{
	return in * (3.0 / 4.0);
}
