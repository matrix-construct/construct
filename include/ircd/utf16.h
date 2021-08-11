// The Construct
//
// Copyright (C) The Construct Developers, Authors & Contributors
// Copyright (C) 2016-2021 Jason Volk <jason@zemos.net>
//
// Permission to use, copy, modify, and/or distribute this software for any
// purpose with or without fee is hereby granted, provided that the above
// copyright notice and this permission notice is present in all copies. The
// full license for this software is available in the LICENSE file.

#pragma once
#define HAVE_IRCD_UTF16_H

/// Unicode Transformation Format (16-bit)
namespace ircd::utf16
{
	// mask all surrogate characters from find_() result
	template<class u8xN> u8xN mask_surrogate(const u8xN found) noexcept;

	// scan for utf-16 surrogates
	template<class u8xN> u8xN find_surrogate(const u8xN input) noexcept;

	// scan for utf-16 surrogates including incomplete sequences truncated
	u8x16 find_surrogate_partial(const u8x16 input) noexcept;

	// decodes one or two surrogates at the front into one or two codepoints
	u32x4 decode_surrogate_aligned_next(const u8x16 input) noexcept;
}

/// The vector returned by find_surrogate() masks the leading character of
/// every valid surrogate (i.e. the '\'). This is a convenience to mask
/// the full surrogate from such a result.
template<class u8xN>
inline u8xN
ircd::utf16::mask_surrogate(const u8xN found)
noexcept
{
	return u8xN
	{
		shl<0x08>(found) |
		shl<0x10>(found) |
		shl<0x18>(found) |
		shl<0x20>(found) |
		shl<0x28>(found) |
		found
	};
}
