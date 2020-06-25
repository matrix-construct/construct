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

decltype(ircd::simd::u8x32_lane_id)
ircd::simd::u8x32_lane_id
{
	0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07,
	0x08, 0x09, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f,
	0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17,
	0x18, 0x19, 0x1a, 0x1b, 0x1c, 0x1d, 0x1e, 0x1f,
};

decltype(ircd::simd::u16x16_lane_id)
ircd::simd::u16x16_lane_id
{
	0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07,
	0x08, 0x09, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f,
};

decltype(ircd::simd::u8x16_lane_id)
ircd::simd::u8x16_lane_id
{
	0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07,
	0x08, 0x09, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f,
};

decltype(ircd::simd::u32x8_lane_id)
ircd::simd::u32x8_lane_id
{
	0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07,
};

decltype(ircd::simd::u16x8_lane_id)
ircd::simd::u16x8_lane_id
{
	0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07,
};

decltype(ircd::simd::u64x4_lane_id)
ircd::simd::u64x4_lane_id
{
	0x00, 0x01, 0x02, 0x03,
};

decltype(ircd::simd::u32x4_lane_id)
ircd::simd::u32x4_lane_id
{
	0x00, 0x01, 0x02, 0x03,
};

decltype(ircd::simd::u64x2_lane_id)
ircd::simd::u64x2_lane_id
{
	0x00, 0x01,
};

decltype(ircd::simd::u256x1_lane_id)
ircd::simd::u256x1_lane_id
{
	0x00,
};

decltype(ircd::simd::u128x1_lane_id)
ircd::simd::u128x1_lane_id
{
	0x00,
};

template<>
ircd::string_view
ircd::simd::str_reg(const mutable_buffer &buf,
                    const u8x16 &a,
                    const uint &fmt)
noexcept
{
	const auto len(::snprintf
	(
		data(buf), size(buf),
		"0x%02x 0x%02x 0x%02x 0x%02x 0x%02x 0x%02x 0x%02x 0x%02x "
		"0x%02x 0x%02x 0x%02x 0x%02x 0x%02x 0x%02x 0x%02x 0x%02x",
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
ircd::simd::str_reg(const mutable_buffer &buf,
                    const u16x8 &a_,
                    const uint &fmt)
noexcept
{
	const u8x16 a(a_);
	const auto len(::snprintf
	(
		data(buf), size(buf),
		"0x%02x%02x 0x%02x%02x 0x%02x%02x 0x%02x%02x "
		"0x%02x%02x 0x%02x%02x 0x%02x%02x 0x%02x%02x",
		a[0x01], a[0x00], a[0x03], a[0x02], a[0x05], a[0x04], a[0x07], a[0x06],
		a[0x09], a[0x08], a[0x0b], a[0x0a], a[0x0d], a[0x0c], a[0x0f], a[0x0e]
	));

	return string_view
	{
		data(buf), size_t(len)
	};
}

template<>
ircd::string_view
ircd::simd::str_reg(const mutable_buffer &buf,
                    const u32x4 &a_,
                    const uint &fmt)
noexcept
{
	const u8x16 a(a_);
	const auto len(::snprintf
	(
		data(buf), size(buf),
		"0x%02x%02x%02x%02x 0x%02x%02x%02x%02x "
		"0x%02x%02x%02x%02x 0x%02x%02x%02x%02x",
		a[0x03], a[0x02], a[0x01], a[0x00], a[0x07], a[0x06], a[0x05], a[0x04],
		a[0x0b], a[0x0a], a[0x09], a[0x08], a[0x0f], a[0x0e], a[0x0d], a[0x0c]
	));

	return string_view
	{
		data(buf), size_t(len)
	};
}

template<>
ircd::string_view
ircd::simd::str_reg(const mutable_buffer &buf,
                    const u64x2 &a_,
                    const uint &fmt)
noexcept
{
	const u8x16 a(a_);
	const auto len(::snprintf
	(
		data(buf), size(buf),
		"0x%02x%02x%02x%02x%02x%02x%02x%02x 0x%02x%02x%02x%02x%02x%02x%02x%02x",
		a[0x07], a[0x06], a[0x05], a[0x04], a[0x03], a[0x02], a[0x01], a[0x00],
		a[0x0f], a[0x0e], a[0x0d], a[0x0c], a[0x0b], a[0x0a], a[0x09], a[0x08]
	));

	return string_view
	{
		data(buf), size_t(len)
	};
}

template<>
ircd::string_view
ircd::simd::str_reg(const mutable_buffer &buf,
                    const u128x1 &a_,
                    const uint &fmt)
noexcept
{
	const u8x16 a(a_);
	const auto len(::snprintf
	(
		data(buf), size(buf),
		"0x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x",
		a[0x0f], a[0x0e], a[0x0d], a[0x0c], a[0x0b], a[0x0a], a[0x09], a[0x08],
		a[0x07], a[0x06], a[0x05], a[0x04], a[0x03], a[0x02], a[0x01], a[0x00]
	));

	return string_view
	{
		data(buf), size_t(len)
	};
}

template<>
ircd::string_view
ircd::simd::str_mem(const mutable_buffer &buf,
                    const u8x16 &a_,
                    const uint &fmt)
noexcept
{
	const u8x16 a(a_);
	const auto len(::snprintf
	(
		data(buf), size(buf),
		"%02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x",
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
ircd::simd::str_mem(const mutable_buffer &buf,
                    const u16x8 &a_,
                    const uint &fmt)
noexcept
{
	const u8x16 a(a_);
	const auto len(::snprintf
	(
		data(buf), size(buf),
		"%02x%02x %02x%02x %02x%02x %02x%02x %02x%02x %02x%02x %02x%02x %02x%02x",
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
ircd::simd::str_mem(const mutable_buffer &buf,
                    const u32x4 &a_,
                    const uint &fmt)
noexcept
{
	const u8x16 a(a_);
	const auto len(::snprintf
	(
		data(buf), size(buf),
		"%02x%02x%02x%02x %02x%02x%02x%02x %02x%02x%02x%02x %02x%02x%02x%02x",
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
ircd::simd::str_mem(const mutable_buffer &buf,
                    const u64x2 &a_,
                    const uint &fmt)
noexcept
{
	const u8x16 a(a_);
	const auto len(::snprintf
	(
		data(buf), size(buf),
		"%02x%02x%02x%02x%02x%02x%02x%02x %02x%02x%02x%02x%02x%02x%02x%02x",
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
ircd::simd::str_mem(const mutable_buffer &buf,
                    const u128x1 &a_,
                    const uint &fmt)
noexcept
{
	const u8x16 a(a_);
	const auto len(::snprintf
	(
		data(buf), size(buf),
		"%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x",
		a[0x00], a[0x01], a[0x02], a[0x03], a[0x04], a[0x05], a[0x06], a[0x07],
		a[0x08], a[0x09], a[0x0a], a[0x0b], a[0x0c], a[0x0d], a[0x0e], a[0x0f]
	));

	return string_view
	{
		data(buf), size_t(len)
	};
}
