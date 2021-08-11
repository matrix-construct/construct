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
#define HAVE_IRCD_UTF8_H

/// Unicode Transformation Format (8-bit)
namespace ircd::utf8
{
	// Get the utf8-encoded length from char32_t (decoded) codepoints
	template<class u32xN> u32xN length(const u32xN codepoints) noexcept;

	// Get the utf8 length at the first byte of each utf8-codepoint; 0x00 sep
	template<> u8x16 length(const u8x16 string) noexcept;

	// Encode char32_t codepoints into respective utf-8 encodings
	template<class u32xN> u32xN encode_sparse(const u32xN codepoints) noexcept;

	// Decode utf-8 string into char32_t unicode codepoints
	u32x16 decode_sparse(const u8x16 string) noexcept;

	// Decode utf-8 string into char32_t unicode codepoints packed left.
	u32x16 decode(const u8x16 string) noexcept;
}

template<>
inline ircd::u8x16
ircd::utf8::length(const u8x16 string)
noexcept
{
	const u8x16 is_single
	(
		string < 0x80
	);

	const u8x16 is_multi
	(
		(string - 0xc2) <= 0x32
	);

	const u8x16 num_trail
	(
		1
		+ ((string >= 0xc0) & 1)
		+ ((string >= 0xe0) & 1)
		+ ((string >= 0xf0) & 1)
	);

	return (is_single & 1) | (is_multi & num_trail);
}
