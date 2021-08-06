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

/// Decodes one or two escaped surrogates (surrogate pair) aligned to the
/// front of the input block. If the surrogates are a pair which decode into
/// a single codepoint, only the first element of the return vector is used;
/// otherwise each surrogate decodes into each element. Three surrogates
/// cannot be decoded at once, so the last two elements are never used.
ircd::u32x4
ircd::utf16::decode_surrogate_aligned_next(const u8x16 input)
noexcept
{
	const u8x16 is_hex[3]
	{
		input >= '0' && input <= '9',
		input >= 'A' && input <= 'F',
		input >= 'a' && input <= 'f',
	};

	const u8x16 hex_nibble
	{
		((input - 0x30) & is_hex[0])
		| ((input - 0x41 + 0x0a) & is_hex[1])
		| ((input - 0x61 + 0x0a) & is_hex[2])
	};

	const u8x16 is_hex_nibble
	{
		is_hex[0] | is_hex[1] | is_hex[2]
	};

	// Masks the starting byte (the '\' char) of each valid surrogate.
	const u8x16 is_surrogate
	{
		(input == '\\') &
		shr<8>(input == 'u') &
		shr<16>(is_hex_nibble) &
		shr<24>(is_hex_nibble) &
		shr<32>(is_hex_nibble) &
		shr<40>(is_hex_nibble)
	};

	// is_surrogate may leave byte[0] and byte[6] (and possibly byte[12] which
	// we don't care about here) as 0xff. Our result will be 4 byte codepoints
	// matching those 6 byte inputs, so we shift the byte[6] over to byte[4]
	// and stiffen the mask about to be generated.
	const u32x4 surrogate_mask
	(
		((u32x4(is_surrogate) & 0xff) | (u32x4(is_surrogate) >> 16)) == 0xffU
	);

	// Decide if one or two surrogates were actually input and assert that
	// between both lanes if so.
	const u32x4 surrogate_deuce
	{
		(surrogate_mask & shr<32>(surrogate_mask)) |
		(surrogate_mask & shl<32>(surrogate_mask))
	};

	// ASCII to integral converion of the upper nibbles
	const u8x16 hex_upper
	{
		shr<16>(hex_nibble)
	};

	// ASCII to integral converion of the lower nibbles
	const u8x16 hex_lower
	{
		shr<24>(hex_nibble)
	};

	// pack upper and lower nibbles into bytes, though these have a space
	// between them when 4 nibbles becomes 2 bytes
	const u8x16 hex_byte
	{
		(hex_upper << 4) | hex_lower
	};

	// Result for one or two unpaired surrogates
	const u32x4 codepoint_unpaired
	(
		u8x16
		{
			hex_byte[2], hex_byte[0], 0, 0,
			hex_byte[8], hex_byte[6], 0, 0,
			0, 0, 0, 0,
			0, 0, 0, 0,
		}
	);

	// Determine if the unpaired codepoints can make a surrogate pair
	const u32x4 surrogate_pair_range
	(
		codepoint_unpaired >= 0xd800U && codepoint_unpaired <= 0xdfffU
	);

	// Mask lane[0] if the codepoints are actually a surrogate pair
	const u32x4 surrogate_paired
	(
		surrogate_pair_range & shr<32>(surrogate_pair_range)
	);

	// Pre-processing shuffle for surrogate pair decode
	const u32x4 codepoint_pre_paired
	{
		shr<16>(codepoint_unpaired) | codepoint_unpaired
	};

	// Decode surrogate pair
	const u32x4 codepoint_paired
	{
		0x10000U +
		((codepoint_pre_paired & 0x000003ffU) << 10) +
		((codepoint_pre_paired & 0x03ff0000U) >> 16)
	};

	// Decide if the codepoint is in the supplementary plane (3+ bytes)
	const u32x4 codepoint_high
	(
		(codepoint_paired > 0xffffU) & surrogate_paired
	);

	// Decide if the codepoint is in the BMP (2- bytes)
	const u32x4 codepoint_low
	{
		(codepoint_paired <= 0xffffU) & ~(shl<32>(codepoint_high))
	};

	// When one surrogate is input, only lane[0]
	const u32x4 ret_codepoint_single
	{
		codepoint_unpaired & ~surrogate_pair_range & ~surrogate_deuce
	};

	// When two surrogates in a pair are input, lane[0] only
	const u32x4 ret_codepoint_paired
	{
		codepoint_paired & (surrogate_paired & surrogate_deuce)
	};

	// When two unrelated surrogates are input, lane[0] and lane[1]
	const u32x4 ret_codepoint_unpaired
	{
		codepoint_unpaired & ~surrogate_pair_range & surrogate_deuce
	};

	static const u32x4
	mask_one { -1U,  0U,  0U,  0U, },
	mask_two { -1U, -1U,  0U,  0U, };

	return 0
	| (ret_codepoint_single & mask_one)
	| (ret_codepoint_paired & mask_one)
	| (ret_codepoint_unpaired & mask_two)
	;
}

namespace ircd::utf16
{
	static const u128x1 full_mask {~u128x1{0}};
	extern const u8x16 truncation_table[6];
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
	const u8x16 is_esc
	(
		input == '\\'
	);

	const u8x16 is_u
	(
		input == 'u'
	);

	const u8x16 hex_nibble[3]
	{
		input >= '0' && input <= '9',
		input >= 'A' && input <= 'F',
		input >= 'a' && input <= 'f',
	};

	const u8x16 is_hex_nibble
	{
		hex_nibble[0] | hex_nibble[1] | hex_nibble[2]
	};

	const u8x16 surrogate_sans[6]
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

	const u8x16 ret
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

namespace ircd::utf16
{
	template u8x16 utf16::find_surrogate<u8x16>(const u8x16) noexcept;
	template u8x32 utf16::find_surrogate<u8x32>(const u8x32) noexcept;

	// Clang-10 is having trouble with this instantiation on aarch64
	#if !defined(__clang__) || !defined(__aarch64__)
	template u8x64 utf16::find_surrogate<u8x64>(const u8x64) noexcept;
	#endif
}

template<class u8xN>
u8xN
ircd::utf16::find_surrogate(const u8xN input)
noexcept
{
	const u8xN hex_nibble[3]
	{
		input >= '0' && input <= '9',
		input >= 'A' && input <= 'F',
		input >= 'a' && input <= 'f',
	};

	const u8xN is_hex_nibble
	{
		hex_nibble[0] | hex_nibble[1] | hex_nibble[2]
	};

	const u8xN is_surrogate
	{
		(input == '\\') &
		shr<8>(input == 'u') &
		shr<16>(is_hex_nibble) &
		shr<24>(is_hex_nibble) &
		shr<32>(is_hex_nibble) &
		shr<40>(is_hex_nibble)
	};

	return is_surrogate;
}

//
// utf8
//

ircd::u32x16
ircd::utf8::decode(const u8x16 in)
noexcept
{
	const u8x16 is_single
	(
		(in & 0x80) == 0
	);

	const u8x16 is_lead
	(
		(in - 0xc2) <= 0x32
	);

	const u8x16 is_trail
	(
		in >= 0x80 && in < 0xbf
	);

	const u8x16 is_head
	(
		is_lead | is_single
	);

	const u8x16 len_mask[3]
	{
		in >= 0xc0, in >= 0xe0, in >= 0xf0,
	};

	const u8x16 expect_trail
	(
		1 + (len_mask[0] & 1) + (len_mask[1] & 1) + (len_mask[2] & 1)
	);

	const u8x16 len
	(
		(is_single & 1) | (is_lead & expect_trail)
	);

	const u8x16 head
	(
		in & is_head
	);

	const u8x16 lead[]
	{
		0x3f & in & is_trail,
		0xff & head & is_single,
		0x1f & head & len_mask[0] & ~len_mask[1],
		0x0f & head & len_mask[1] & ~len_mask[2],
		0x07 & head & len_mask[2],
	};

	u8x16 full;
	for(uint i(0); i < 16; ++i)
		full[i] = lead[len[i]][i];

	u8x16 shift {len & is_head};
	shift |= (shl<0x20>(len) == 4) & 0;
	shift |= (shl<0x20>(len) == 3) & 0;
	shift |= (shl<0x20>(len) == 2) & 0;
	shift |= (shl<0x20>(len) == 1) & 0;
	shift |= (shl<0x18>(len) == 4) & 1;
	shift |= (shl<0x18>(len) == 3) & 0;
	shift |= (shl<0x18>(len) == 2) & 0;
	shift |= (shl<0x18>(len) == 1) & 0;
	shift |= (shl<0x10>(len) == 4) & 2;
	shift |= (shl<0x10>(len) == 3) & 1;
	shift |= (shl<0x10>(len) == 2) & 0;
	shift |= (shl<0x10>(len) == 1) & 0;
	shift |= (shl<0x08>(len) == 4) & 3;
	shift |= (shl<0x08>(len) == 3) & 2;
	shift |= (shl<0x08>(len) == 2) & 1;
	shift |= (shl<0x08>(len) == 1) & 0;
	shift -= 1U;
	shift &= 0x03U;
	shift *= 6U;

	const u32x16 val
	{
		lane_cast<u32x16>(full) << lane_cast<u32x16>(shift)
	};

	const u8x16 incr
	(
		(shift == 0) & 1U
	);

	u8x16 idx {0};
	for(uint i(1); i < 16; ++i)
		idx[i] = idx[i - 1] + incr[i - 1];

	u32x16 ret {0};
	for(uint i(0); i < 16; ++i)
		ret[idx[i]] |= val[i];

	return ret;
}

namespace ircd::utf8
{
	template<class u32xN> static u32xN _encode_sparse(const u32xN codepoint) noexcept;
}

template<>
ircd::u32x4
ircd::utf8::encode_sparse(const u32x4 codepoint)
noexcept
{
	return _encode_sparse(codepoint);
}

template<>
ircd::u32x8
ircd::utf8::encode_sparse(const u32x8 codepoint)
noexcept
#ifdef __AVX2__
{
	return _encode_sparse(codepoint);
}
#else // This block is only effective for GCC. Clang performs this automatically.
{
	u32x4 cp[2];
	for(size_t i(0); i < 2; ++i)
		for(size_t j(0); j < 4; ++j)
			cp[i][j] = codepoint[i * 4 + j];

	cp[0] = _encode_sparse(cp[0]);
	cp[1] = _encode_sparse(cp[1]);

	u32x8 ret;
	for(size_t i(0); i < 2; ++i)
		for(size_t j(0); j < 4; ++j)
			ret[i * 4 + j] = cp[i][j];

	return ret;
}
#endif

template<>
ircd::u32x16
ircd::utf8::encode_sparse(const u32x16 codepoint)
noexcept
#ifdef __AVX512F__
{
	return _encode_sparse(codepoint);
}
#else // This block is only effective for GCC. Clang performs this automatically.
{
	u32x8 cp[2];
	for(size_t i(0); i < 2; ++i)
		for(size_t j(0); j < 8; ++j)
			cp[i][j] = codepoint[i * 8 + j];

	cp[0] = encode_sparse(cp[0]);
	cp[1] = encode_sparse(cp[1]);

	u32x16 ret;
	for(size_t i(0); i < 2; ++i)
		for(size_t j(0); j < 8; ++j)
			ret[i * 8 + j] = cp[i][j];

	return ret;
}
#endif

/// Transform multiple char32_t codepoints to their utf-8 encodings in
/// parallel, returning a sparse result in each char32_t (this does not
/// compress the result down).
template<class u32xN>
inline u32xN
ircd::utf8::_encode_sparse(const u32xN codepoint)
noexcept
{
	const u32xN len
	{
		length(codepoint)
	};

	const u32xN enc_2
	{
		(((codepoint >> 6) | 0xc0) & 0xff) // byte[0]
		| ((((codepoint & 0x3f) | 0x80) & 0xff) << 8) // byte[1]
	};

	const u32xN enc_3
	{
		(((codepoint >> 12) | 0xe0) & 0xff) | // byte[0]
		(((((codepoint >> 6) & 0x3f) | 0x80) & 0xff) << 8) | // byte[1]
		((((codepoint & 0x3f) | 0x80) & 0xff) << 16) // byte[2]
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

namespace ircd::utf8
{
	template<class u32xN> static u32xN _length(const u32xN codepoint) noexcept;
}

template<>
ircd::u32x4
ircd::utf8::length(const u32x4 codepoint)
noexcept
{
	return _length(codepoint);
}

template<>
ircd::u32x8
ircd::utf8::length(const u32x8 codepoint)
noexcept
#ifdef __AVX2__
{
	return _length(codepoint);
}
#else // This block is only effective for GCC. Clang performs this automatically.
{
	u32x4 cp[2];
	for(size_t i(0); i < 2; ++i)
		for(size_t j(0); j < 4; ++j)
			cp[i][j] = codepoint[i * 4 + j];

	cp[0] = _length(cp[0]);
	cp[1] = _length(cp[1]);

	u32x8 ret;
	for(size_t i(0); i < 2; ++i)
		for(size_t j(0); j < 4; ++j)
			ret[i * 4 + j] = cp[i][j];

	return ret;
}
#endif

template<>
ircd::u32x16
ircd::utf8::length(const u32x16 codepoint)
noexcept
#ifdef __AVX512F__
{
	return _length(codepoint);
}
#else // This block is only effective for GCC. Clang performs this automatically.
{
	u32x8 cp[2];
	for(size_t i(0); i < 2; ++i)
		for(size_t j(0); j < 8; ++j)
			cp[i][j] = codepoint[i * 8 + j];

	cp[0] = length(cp[0]);
	cp[1] = length(cp[1]);

	u32x16 ret;
	for(size_t i(0); i < 2; ++i)
		for(size_t j(0); j < 8; ++j)
			ret[i * 8 + j] = cp[i][j];

	return ret;
}
#endif

/// Determine the utf-8 encoding length of multiple codepoints in parallel.
/// The input vector char32_t codepoints and the output yields an integer
/// of 0-4 for each lane.
template<class u32xN>
inline u32xN
ircd::utf8::_length(const u32xN codepoint)
noexcept
{
	const u32xN len[5]
	{
		// length 1
		codepoint <= 0x7f,

		// length 2
		codepoint <= 0x7ff && codepoint > 0x7f,

		// length 3 low
		codepoint <= 0xd7ff && codepoint > 0x7ff,

		// length 3 high
		codepoint <= 0xffff && codepoint > 0xdfff,

		// length 4
		codepoint <= 0x10ffff && codepoint > 0xffff,
	};

	[[gnu::unused]] // Preserved here for future reference
	const u32xN len_3_err
	(
		codepoint <= 0xdfff && codepoint > 0xd7ff
	);

	[[gnu::unused]] // Preserved here for future reference
	const u32xN len_err
	{
		(codepoint > 0x10ffff) | len_3_err
	};

	return 0
	| (len[0] & 1)
	| (len[1] & 2)
	| (len[2] & 3)
	| (len[3] & 3)
	| (len[4] & 4)
	;
}
