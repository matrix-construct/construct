// The Construct
//
// Copyright (C) The Construct Developers, Authors & Contributors
// Copyright (C) 2016-2020 Jason Volk <jason@zemos.net>
//
// Permission to use, copy, modify, and/or distribute this software for any
// purpose with or without fee is hereby granted, provided that the above
// copyright notice and this permission notice is present in all copies. The
// full license for this software is available in the LICENSE file.

/// Convert utf-16 two-byte surrogates (in big-endian) to char32_t codepoints
/// in parallel. The result vector is twice the size as the input; no template
/// is offered yet, just the dimensions someone needed for somewhere.
ircd::u32x8
ircd::utf16::convert_u32x8(const u8x16 &string)
noexcept
{
	return u32x8
	{
		string[0x01] | (u32(string[0x00]) << 8),
		string[0x03] | (u32(string[0x02]) << 8),
		string[0x05] | (u32(string[0x04]) << 8),
		string[0x07] | (u32(string[0x06]) << 8),
		string[0x09] | (u32(string[0x08]) << 8),
		string[0x0b] | (u32(string[0x0a]) << 8),
		string[0x0d] | (u32(string[0x0c]) << 8),
		string[0x0f] | (u32(string[0x0e]) << 8),
	};
}

/// Transform multiple char32_t codepoints to their utf-8 encodings in
/// parallel, returning a sparse result in each char32_t (this does not
/// compress the result down).
template<class u32xN>
u32xN
ircd::utf8::encode(const u32xN &codepoint)
noexcept
{
	const u32xN len
	{
		length(codepoint)
	};

	const u32xN enc_2
	{
		(((codepoint >> 6) | 0xc0) & 0xff) // byte[0]
		| ((((codepoint & 0x3f) | 0x80) &0xff) << 8) // byte[1]
	};

	const u32xN enc_3
	{
		(((codepoint >> 12) | 0xe0) & 0xff) | // byte[0]
		(((((codepoint >> 6) & 0x3f) | 0x80) & 0xff) << 8) | // byte[1]
		((((codepoint & 0x3f) | 0x80) & 0xff) << 16) // byte[3]
	};

	const u32xN enc_4
	{
		(((codepoint >> 18) | 0xf0) & 0xff) | // byte[0]
		(((((codepoint >> 12) & 0x3f) | 0x80) & 0xff) << 8) | // byte[1]
		(((((codepoint >> 6) & 0x3f) | 0x80) & 0xff) << 16) | // byte[2]
		((((codepoint & 0x3f) | 0x80) & 0xff) << 24) // byte[3]
	};

	return 0
	| ((len == 0) & 0xFFFD)
	| ((len == 1) & codepoint)
	| ((len == 2) & enc_2)
	| ((len == 3) & enc_3)
	| ((len == 4) & enc_4)
	;
}

/// Determine the utf-8 encoding length of multiple codepoints in parallel.
/// The input vector char32_t codepoints and the output yields an integer
/// of 0-4 for each lane.
template<class u32xN>
u32xN
ircd::utf8::length(const u32xN &codepoint)
noexcept
{
	const u32xN
	length_1      { codepoint <= 0x7f                               },
	length_2      { codepoint <= 0x7ff && codepoint > 0x7f          },
	length_3_lo   { codepoint <= 0xd7ff && codepoint > 0x7ff        },
	length_3_hi   { codepoint <= 0xffff && codepoint > 0xdfff       },
	length_4      { codepoint <= 0x10ffff && codepoint > 0xffff     };

	[[gnu::unused]] const u32xN // Preserved here for future reference
	length_3_err  { codepoint <= 0xdfff && codepoint > 0xd7ff       },
	length_err    { (codepoint > 0x10ffff) | length_3_err           };

	return 0
	| (length_1 & 1)
	| (length_2 & 2)
	| (length_3_lo & 3)
	| (length_3_hi & 3)
	| (length_4 & 4)
	;
}
