// The Construct
//
// Copyright (C) The Construct Developers, Authors & Contributors
// Copyright (C) 2016-2020 Jason Volk <jason@zemos.net>
//
// Permission to use, copy, modify, and/or distribute this software for any
// purpose with or without fee is hereby granted, provided that the above
// copyright notice and this permission notice is present in all copies. The
// full license for this software is available in the LICENSE file.

#include <ircd/simd.h>

template<>
ircd::string_view
ircd::simd::print_lane(const mutable_buffer &buf,
                       const u8x16 &a)
noexcept
{
	const auto len(::snprintf
	(
		data(buf), size(buf),
		"[%02x|%02x|%02x|%02x|%02x|%02x|%02x|%02x|%02x|%02x|%02x|%02x|%02x|%02x|%02x|%02x]",
		a[0x00], a[0x01], a[0x02], a[0x03], a[0x04], a[0x05], a[0x06], a[0x07],
		a[0x08], a[0x09], a[0x0a], a[0x0b], a[0x0c], a[0x0d], a[0x0e], a[0x0f]
	));

	return string_view
	{
		data(buf), size_t(len)
	};
}

template<>
ircd::string_view
ircd::simd::print_lane(const mutable_buffer &buf,
                       const u8x32 &a)
noexcept
{
    const auto len(::snprintf
    (
		data(buf), size(buf),
		"[%02x|%02x|%02x|%02x|%02x|%02x|%02x|%02x|%02x|%02x|%02x|%02x|%02x|%02x|%02x|%02x"
		"|%02x|%02x|%02x|%02x|%02x|%02x|%02x|%02x|%02x|%02x|%02x|%02x|%02x|%02x|%02x|%02x]",
		a[0x00], a[0x01], a[0x02], a[0x03], a[0x04], a[0x05], a[0x06], a[0x07],
		a[0x08], a[0x09], a[0x0a], a[0x0b], a[0x0c], a[0x0d], a[0x0e], a[0x0f],
		a[0x10], a[0x11], a[0x12], a[0x13], a[0x14], a[0x15], a[0x16], a[0x17],
		a[0x18], a[0x19], a[0x1a], a[0x1b], a[0x1c], a[0x1d], a[0x1e], a[0x1f]
	));

	return string_view
	{
		data(buf), size_t(len)
	};
}
