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
#define HAVE_IRCD_LEB128_H

/// Little Endian Base 128 (unsigned) tool suite.
namespace ircd::uleb128
{
	template<class T> constexpr size_t len(const T &) noexcept;
	template<class T> constexpr T encode(T) noexcept;
	template<class T> constexpr T decode(T) noexcept;
}

/// Generic template to decode an unsigned LEB128. For constexprs this
/// produces zero code. Inlined it is branchless, and reasonable. Unfortunately
/// too much unrolling can be unwieldy for inlining, but the use cases tend to
/// be very high in call frequency to decode many bytes: this is why we have
/// some template specializations with platform-specific optimizations;
/// otherwise, this template is the default.
///
/// Note that the input can contain junk above the encoded integer, which will
/// be ignored. Decoding starts at the first byte of the input (regardless of
/// type T) and continues until the first byte which has its MSB clear (limited
/// by the size of the type T); bytes after the terminating byte are ignored.
template<class T>
inline constexpr T
ircd::uleb128::decode(T val)
noexcept
{
	constexpr const T control_mask {0x80}, content_mask {0x7F};

	T ret(0); // destination
	uint8_t flag(content_mask);
	for(size_t i(0); i < sizeof(T); ++i)
	{
		// Sample the 7 bits of content into b. b will be zero if the control
		// flag was previously cleared, otherwise the control flag is the mask
		// for the 7 bits.
		T b(val & flag);

		// test if the 8th bit is zero or one; if zero, this was the last byte
		// and the control flag is cleared.
		flag &= ~(val & control_mask);

		// Consume this byte off the source data.
		val >>= 8;

		// Shift the acquired content to its final destination offset.
		b <<= 7 * i;

		// Merge this byte with the destination.
		ret |= b;
	}

	return ret;
}

template<class T>
inline constexpr T
ircd::uleb128::encode(T val)
noexcept
{
	constexpr const T content_mask {0x7F};

	T ret(0); // destination
	for(size_t i(0); i < sizeof(T); ++i)
	{
		// Sample the lowest 7 bits of the input.
		T b(val & content_mask);

		// Consume 7 bits off the input.
		val >>= 7;

		// Set the high order bit on this byte if the input still has more
		// left to encode after this iteration.
		b |= T(bool(val)) << 7;

		// Shift the content to its final destination offset.
		b <<= 8 * i;

		// Merge this byte with destination.
		ret |= b;
	}

	return ret;
}

template<class T>
inline constexpr size_t
ircd::uleb128::len(const T &val)
noexcept
{
	constexpr const T control_mask {0x80};

	size_t i(0);
	for(; i < sizeof(T); ++i)
		if(~val & (control_mask << (i * 8)))
			break;

	return i + 1;
}
