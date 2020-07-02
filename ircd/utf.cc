// The Construct
//
// Copyright (C) The Construct Developers, Authors & Contributors
// Copyright (C) 2016-2020 Jason Volk <jason@zemos.net>
//
// Permission to use, copy, modify, and/or distribute this software for any
// purpose with or without fee is hereby granted, provided that the above
// copyright notice and this permission notice is present in all copies. The
// full license for this software is available in the LICENSE file.

//
// utf16
//

namespace ircd::utf16
{
	static const u128x1 full_mask {~u128x1{0}};
	extern const u128x1 truncation_table[6];
}

decltype(ircd::utf16::truncation_table)
ircd::utf16::truncation_table
{
	~shl<0x30>(~full_mask),
	~shl<0x28>(~full_mask),
	~shl<0x20>(~full_mask),
	~shl<0x18>(~full_mask),
	~shl<0x10>(~full_mask),
	~shl<0x08>(~full_mask),
};

/// scan for utf-16 surrogates including incomplete sequences truncated
/// by the end of the input; also matches a single trailing slash.
ircd::u8x16
ircd::utf16::find_surrogate_partial(const u8x16 input)
noexcept
{
	const u128x1 is_esc
	{
		input == '\\'
	};

	const u128x1 is_u
	{
		input == 'u'
	};

	const u128x1 is_hex_nibble
	{
		(input >= '0' && input <= '9') ||
		(input >= 'A' && input <= 'F') ||
		(input >= 'a' && input <= 'f')
	};

	const u128x1 surrogate_sans[6]
	{
		// complete
		is_esc
		& shr<8>(is_u)
		& shr<16>(is_hex_nibble) & shr<24>(is_hex_nibble)
		& shr<32>(is_hex_nibble) & shr<40>(is_hex_nibble),

		// sans 1
		is_esc
		& shr<8>(is_u)
		& shr<16>(is_hex_nibble) & shr<24>(is_hex_nibble)
		& shr<32>(is_hex_nibble),

		// sans 2
		is_esc
		& shr<8>(is_u)
		& shr<16>(is_hex_nibble) & shr<24>(is_hex_nibble),

		// sans 3
		is_esc
		& shr<8>(is_u)
		& shr<16>(is_hex_nibble),

		// sans 4
		is_esc
		& shr<8>(is_u),

		// sans 5
		is_esc,
	};

	const u128x1 ret
	{
		(surrogate_sans[0] & truncation_table[0]) |
		(surrogate_sans[1] & truncation_table[1]) |
		(surrogate_sans[2] & truncation_table[2]) |
		(surrogate_sans[3] & truncation_table[3]) |
		(surrogate_sans[4] & truncation_table[4]) |
		(surrogate_sans[5] & truncation_table[5])
	};

	return ret;
}

ircd::u8x16
ircd::utf16::find_surrogate(const u8x16 input)
noexcept
{
	const u128x1 is_hex_nibble
	{
		(input >= '0' && input <= '9') ||
		(input >= 'A' && input <= 'F') ||
		(input >= 'a' && input <= 'f')
	};

	const auto is_surrogate
	{
		u128x1(input == '\\') &
		shr<8>(u128x1(input == 'u')) &
		shr<16>(is_hex_nibble) &
		shr<24>(is_hex_nibble) &
		shr<32>(is_hex_nibble) &
		shr<40>(is_hex_nibble)
	};

	return is_surrogate;
}

/// Convert utf-16 two-byte surrogates (in big-endian) to char32_t codepoints
/// in parallel. The result vector is twice the size as the input; no template
/// is offered yet, just the dimensions someone needed for somewhere.
ircd::u32x8
ircd::utf16::convert_u32x8(const u8x16 string)
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

//
// utf8
//

ircd::u32x16
ircd::utf8::decode(const u8x16 string)
noexcept
{
	const u32x16 in
	{
		simd::lane_cast<u32x16>(string)
	};

	const u32x16 is_single
	{
		(in & 0x80) == 0
	};

	const u32x16 is_lead
	{
		(in - 0xc2) <= 0x32
	};

	const u32x16 is_trail
	{
		in >= 0x80 && in < 0xbf
	};

	const u32x16 expect_trail
	{
		(((in >= 0xe0) & 1) + ((in >= 0xf0) & 1) + 1) & is_lead
	};

	const u32x16 expect_length
	{
		expect_trail + 1
	};

	const u32x16 shift[4]
	{
		in << 0,
		in << 8,
		in << 16,
		in << 24,
	};

	const u32x16 multibyte_packs
	{
		in[0x00] | shift[0x01][0x01] | shift[0x02][0x02] | shift[0x03][0x03],
		in[0x01] | shift[0x01][0x02] | shift[0x02][0x03] | shift[0x03][0x04],
		in[0x02] | shift[0x01][0x03] | shift[0x02][0x04] | shift[0x03][0x05],
		in[0x03] | shift[0x01][0x04] | shift[0x02][0x05] | shift[0x03][0x06],
		in[0x04] | shift[0x01][0x05] | shift[0x02][0x06] | shift[0x03][0x07],
		in[0x05] | shift[0x01][0x06] | shift[0x02][0x07] | shift[0x03][0x08],
		in[0x06] | shift[0x01][0x07] | shift[0x02][0x08] | shift[0x03][0x09],
		in[0x07] | shift[0x01][0x08] | shift[0x02][0x09] | shift[0x03][0x0a],
		in[0x08] | shift[0x01][0x09] | shift[0x02][0x0a] | shift[0x03][0x0b],
		in[0x09] | shift[0x01][0x0a] | shift[0x02][0x0b] | shift[0x03][0x0c],
		in[0x0a] | shift[0x01][0x0b] | shift[0x02][0x0c] | shift[0x03][0x0d],
		in[0x0b] | shift[0x01][0x0c] | shift[0x02][0x0d] | shift[0x03][0x0e],
		in[0x0c] | shift[0x01][0x0d] | shift[0x02][0x0e] | shift[0x03][0x0f],
		in[0x0d] | shift[0x01][0x0e] | shift[0x02][0x0f] | shift[0x03][0x0f],
		in[0x0e] | shift[0x01][0x0f] | shift[0x02][0x0f] | shift[0x03][0x0f],
		in[0x0f] | shift[0x01][0x0f] | shift[0x02][0x0f] | shift[0x03][0x0f],
	};

	const u32x16 multibyte
	{
		0
		| (multibyte_packs & (expect_length == 1) & 0x000000ffU)
		| (multibyte_packs & (expect_length == 2) & 0x0000ffffU)
		| (multibyte_packs & (expect_length == 3) & 0x00ffffffU)
		| (multibyte_packs & (expect_length == 4) & 0xffffffffU)
	};

	const u32x16 integers
	{
		(in & is_single) | (multibyte & is_lead)
	};

	return integers;
}

/// Transform multiple char32_t codepoints to their utf-8 encodings in
/// parallel, returning a sparse result in each char32_t (this does not
/// compress the result down).
ircd::u32x16
ircd::utf8::encode(const u32x16 codepoint)
noexcept
{
	const u32x16 len
	{
		length(codepoint)
	};

	const u32x16 enc_2
	{
		(((codepoint >> 6) | 0xc0) & 0xff) // byte[0]
		| ((((codepoint & 0x3f) | 0x80) &0xff) << 8) // byte[1]
	};

	const u32x16 enc_3
	{
		(((codepoint >> 12) | 0xe0) & 0xff) | // byte[0]
		(((((codepoint >> 6) & 0x3f) | 0x80) & 0xff) << 8) | // byte[1]
		((((codepoint & 0x3f) | 0x80) & 0xff) << 16) // byte[3]
	};

	const u32x16 enc_4
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

namespace ircd::utf8
{
	template<class u32xN> static u32xN _length(const u32xN codepoint) noexcept;
}

ircd::u32x4
ircd::utf8::length(const u32x4 codepoint)
noexcept
{
	return _length(codepoint);
}

ircd::u32x8
ircd::utf8::length(const u32x8 codepoint)
noexcept
{
	return _length(codepoint);
}

ircd::u32x16
ircd::utf8::length(const u32x16 codepoint)
noexcept
{
	return _length(codepoint);
}

/// Determine the utf-8 encoding length of multiple codepoints in parallel.
/// The input vector char32_t codepoints and the output yields an integer
/// of 0-4 for each lane.
template<class u32xN>
u32xN
ircd::utf8::_length(const u32xN codepoint)
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
