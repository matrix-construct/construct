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
#define HAVE_IRCD_UTF_H

/// Unicode Transformation Format
namespace ircd::utf
{
	IRCD_EXCEPTION(ircd::error, error)
}

/// Unicode Transformation Format (8-bit)
namespace ircd::utf8
{
	u32x16 length(const u32x16 codepoints) noexcept;
	u32x16 encode(const u32x16 codepoints) noexcept;
	u32x16 decode(const u8x16 string) noexcept;
}

/// Unicode Transformation Format (16-bit)
namespace ircd::utf16
{
	// convert packed big endian integer pairs to char32_t;
	u32x8 convert_u32x8(const u8x16 pairs) noexcept;

	// mask all surrogate characters from find_() result
	u8x16 mask_surrogate(const u8x16 found) noexcept;

	// scan for utf-16 surrogates
	u8x16 find_surrogate(const u8x16 input) noexcept;

	// scan for utf-16 surrogates including incomplete sequences truncated
	// by the end of the input; also matches a single trailing slash.
	u8x16 find_surrogate_partial(const u8x16 input) noexcept;
}

inline ircd::u8x16
ircd::utf16::mask_surrogate(const u8x16 found)
noexcept
{
	return u128x1
	{
		shl<0x08>(u128x1(found)) |
		shl<0x10>(u128x1(found)) |
		shl<0x18>(u128x1(found)) |
		shl<0x20>(u128x1(found)) |
		shl<0x28>(u128x1(found)) |
		found
	};
}
