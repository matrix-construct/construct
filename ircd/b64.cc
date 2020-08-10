// The Construct
//
// Copyright (C) The Construct Developers, Authors & Contributors
// Copyright (C) 2016-2020 Jason Volk <jason@zemos.net>
//
// Permission to use, copy, modify, and/or distribute this software for any
// purpose with or without fee is hereby granted, provided that the above
// copyright notice and this permission notice is present in all copies. The
// full license for this software is available in the LICENSE file.

#pragma GCC visibility push(internal)
namespace ircd::b64
{
	constexpr char pad
	{
		'='
	};

	[[gnu::aligned(64)]]
	extern const u8
	encode_permute_tab[64],
	encode_shift_ctrl[64],
	decode_permute_tab[64],
	decode_permute_tab_le[64];

	[[gnu::aligned(64)]]
	extern const i32
	decode_dictionary[256];

	static u8x64 decode_block(const u8x64 in) noexcept;

	template<const u8 (&dict)[64]>
	static u8x64 encode_block(const u8x64 in) noexcept;
}
#pragma GCC visibility pop

decltype(ircd::b64::dict_rfc1421)
ircd::b64::dict_rfc1421
{
	'A', 'B', 'C', 'D', 'E', 'F', 'G', 'H', 'I', 'J', 'K', 'L', 'M', 'N', 'O', 'P',
	'Q', 'R', 'S', 'T', 'U', 'V', 'W', 'X', 'Y', 'Z', 'a', 'b', 'c', 'd', 'e', 'f',
	'g', 'h', 'i', 'j', 'k', 'l', 'm', 'n', 'o', 'p', 'q', 'r', 's', 't', 'u', 'v',
	'w', 'x', 'y', 'z', '0', '1', '2', '3', '4', '5', '6', '7', '8', '9', '+', '/',
};

decltype(ircd::b64::dict_rfc3501)
ircd::b64::dict_rfc3501
{
	'A', 'B', 'C', 'D', 'E', 'F', 'G', 'H', 'I', 'J', 'K', 'L', 'M', 'N', 'O', 'P',
	'Q', 'R', 'S', 'T', 'U', 'V', 'W', 'X', 'Y', 'Z', 'a', 'b', 'c', 'd', 'e', 'f',
	'g', 'h', 'i', 'j', 'k', 'l', 'm', 'n', 'o', 'p', 'q', 'r', 's', 't', 'u', 'v',
	'w', 'x', 'y', 'z', '0', '1', '2', '3', '4', '5', '6', '7', '8', '9', '+', ',',
};

decltype(ircd::b64::dict_rfc4648)
ircd::b64::dict_rfc4648
{
	'A', 'B', 'C', 'D', 'E', 'F', 'G', 'H', 'I', 'J', 'K', 'L', 'M', 'N', 'O', 'P',
	'Q', 'R', 'S', 'T', 'U', 'V', 'W', 'X', 'Y', 'Z', 'a', 'b', 'c', 'd', 'e', 'f',
	'g', 'h', 'i', 'j', 'k', 'l', 'm', 'n', 'o', 'p', 'q', 'r', 's', 't', 'u', 'v',
	'w', 'x', 'y', 'z', '0', '1', '2', '3', '4', '5', '6', '7', '8', '9', '-', '_',
};

decltype(ircd::b64::decode_dictionary)
ircd::b64::decode_dictionary
{
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, // 7
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, // 15
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, // 23
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, // 31
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, // 39
	0x00, 0x00, 0x00,   62,   63,   62, 0x00,   63, // 47
	  52,   53,   54,   55,   56,   57,   58,   59, // 55
	  60,   61, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, // 63
	0x00,    0,    1,    2,    3,    4,    5,    6, // 71
	   7,    8,    9,   10,   11,   12,   13,   14, // 79
	  15,   16,   17,   18,   19,   20,   21,   22, // 87
	  23,   24,   25, 0x00, 0x00, 0x00, 0x00,   63, // 95
	0x00,   26,   27,   28,   29,   30,   31,   32, // 103
	  33,   34,   35,   36,   37,   38,   39,   40, // 111
	  41,   42,   43,   44,   45,   46,   47,   48, // 119
	  49,   50,   51, 0x00, 0x00, 0x00, 0x00, 0x00, // 127
};

decltype(ircd::b64::decode_permute_tab)
ircd::b64::decode_permute_tab
{
	 6,  0,  1,  2,  9, 10,  4,  5, 12, 13, 14,  8, 22, 16, 17, 18,
	25, 26, 20, 21, 28, 29, 30, 24, 38, 32, 33, 34, 41, 42, 36, 37,
	44, 45, 46, 40, 54, 48, 49, 50, 57, 58, 52, 53, 60, 61, 62, 56,
	 0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
};

/// byte-order swapped for each 32-bit word from above
decltype(ircd::b64::decode_permute_tab_le)
ircd::b64::decode_permute_tab_le
{
	 2,  1,  0,  6,  5,  4, 10,  9,  8, 14, 13, 12, 18, 17, 16, 22,
	21, 20, 26, 25, 24, 30, 29, 28, 34, 33, 32, 38, 37, 36, 42, 41,
	40, 46, 45, 44, 50, 49, 48, 54, 53, 52, 58, 57, 56, 62, 61, 60,
	 0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
};

/// For vpermb
decltype(ircd::b64::encode_permute_tab)
ircd::b64::encode_permute_tab
{
	 0 + 1,    0 + 0,    0 + 2,    0 + 1,    3 + 1,    3 + 0,    3 + 2,    3 + 1,
	 6 + 1,    6 + 0,    6 + 2,    6 + 1,    9 + 1,    9 + 0,    9 + 2,    9 + 1,
	12 + 1,   12 + 0,   12 + 2,   12 + 1,   15 + 1,   15 + 0,   15 + 2,   15 + 1,
	18 + 1,   18 + 0,   18 + 2,   18 + 1,   21 + 1,   21 + 0,   21 + 2,   21 + 1,
	24 + 1,   24 + 0,   24 + 2,   24 + 1,   27 + 1,   27 + 0,   27 + 2,   27 + 1,
	30 + 1,   30 + 0,   30 + 2,   30 + 1,   33 + 1,   33 + 0,   33 + 2,   33 + 1,
	36 + 1,   36 + 0,   36 + 2,   36 + 1,   39 + 1,   39 + 0,   39 + 2,   39 + 1,
	42 + 1,   42 + 0,   42 + 2,   42 + 1,   45 + 1,   45 + 0,   45 + 2,   45 + 1,
};

/// For vpmultishiftqb
decltype(ircd::b64::encode_shift_ctrl)
ircd::b64::encode_shift_ctrl
{
	(10 +  0),  ( 4 +  0),  (22 +  0),  (16 +  0),  (10 + 32),  ( 4 + 32),  (22 + 32),  (16 + 32),
	(10 +  0),  ( 4 +  0),  (22 +  0),  (16 +  0),  (10 + 32),  ( 4 + 32),  (22 + 32),  (16 + 32),
	(10 +  0),  ( 4 +  0),  (22 +  0),  (16 +  0),  (10 + 32),  ( 4 + 32),  (22 + 32),  (16 + 32),
	(10 +  0),  ( 4 +  0),  (22 +  0),  (16 +  0),  (10 + 32),  ( 4 + 32),  (22 + 32),  (16 + 32),
	(10 +  0),  ( 4 +  0),  (22 +  0),  (16 +  0),  (10 + 32),  ( 4 + 32),  (22 + 32),  (16 + 32),
	(10 +  0),  ( 4 +  0),  (22 +  0),  (16 +  0),  (10 + 32),  ( 4 + 32),  (22 + 32),  (16 + 32),
	(10 +  0),  ( 4 +  0),  (22 +  0),  (16 +  0),  (10 + 32),  ( 4 + 32),  (22 + 32),  (16 + 32),
	(10 +  0),  ( 4 +  0),  (22 +  0),  (16 +  0),  (10 + 32),  ( 4 + 32),  (22 + 32),  (16 + 32),
};

/// Encoding in to base64 at out. Out must be 1.33+ larger than in
/// padding is not present in the returned view.
template<const ircd::u8 (&dict)[64]>
ircd::string_view
ircd::b64::encode(const mutable_buffer &out,
                  const const_buffer &in)
noexcept
{
	const auto pads
	{
		(3 - size(in) % 3) % 3
	};

	const auto encoded
	{
		encode_unpadded<dict>(out, in)
	};

	assert(size(encoded) + pads <= size(out));
	memset(data(out) + size(encoded), b64::pad, pads);

	const auto len
	{
		size(encoded) + pads
	};

	return string_view
	{
		data(out), len
	};
}
template ircd::string_view ircd::b64::encode<ircd::b64::standard>(const mutable_buffer &, const const_buffer &) noexcept;
template ircd::string_view ircd::b64::encode<ircd::b64::urlsafe>(const mutable_buffer &, const const_buffer &) noexcept;

/// Encoding in to base64 at out. Out must be 1.33+ larger than in.
template<const ircd::u8 (&dict)[64]>
ircd::string_view
ircd::b64::encode_unpadded(const mutable_buffer &out,
                           const const_buffer &in)
noexcept
{
	const size_t res_len
	{
		size_t(ceil(size(in) * (4.0 / 3.0)))
	};

	const size_t out_len
	{
		std::min(res_len, size(out))
	};

	size_t i(0), j(0);
	for(; i + 1 < (size(in) / 48) && i < (out_len / 64); ++i)
	{
		// Destination is indexed at 64 byte stride
		const auto di
		{
			reinterpret_cast<u512x1_u *__restrict__>(data(out) + (i * 64))
		};

		// Source is indexed at 48 byte stride
		const auto si
		{
			reinterpret_cast<const u512x1_u *__restrict__>(data(in) + (i * 48))
		};

		*di = encode_block<dict>(*si);
	}

	for(; i * 48 < size(in); ++i)
	{
		u8x64 block{0};
		for(j = 0; i * 48 + j < size(in); ++j)
			block[j] = in[i * 48 + j];

		block = encode_block<dict>(block);
		for(j = 0; i * 64 + j < out_len; ++j)
			out[i * 64 + j] = block[j];
	}

	return string_view
	{
		data(out), out_len
	};
}
template ircd::string_view ircd::b64::encode_unpadded<ircd::b64::standard>(const mutable_buffer &, const const_buffer &) noexcept;
template ircd::string_view ircd::b64::encode_unpadded<ircd::b64::urlsafe>(const mutable_buffer &, const const_buffer &) noexcept;

/// Returns 64 base64-encoded characters from 48 input characters. For any
/// inputs less than 48 characters trail with null characters; caller computes
/// result size. The following operations are performed on each triple of input
/// characters resulting in four output characters:
/// 0.  in[0] / 4;
/// 1.  (in[1] / 16) + ((in[0] * 16) % 64);
/// 2.  ((in[1] * 4) % 64) + (in[2] / 64);
/// 3.  in[2] % 64;
/// Based on https://arxiv.org/pdf/1910.05109 (and earlier work). No specific
/// intrinsics are used here; instead we recite a kotodama divination known
/// as "vector extensions" which by chance is visible to humans as C syntax.
template<const ircd::u8 (&dict)[64]>
ircd::u8x64
ircd::b64::encode_block(const u8x64 in)
noexcept
{
	size_t i, j, k;

	// vpermb
	u8x64 _perm;
	for(k = 0; k < 64; ++k)
		_perm[k] = in[encode_permute_tab[k]];

	// TODO: currently does not achieve vpmultshiftqb on avx512vbmi
	u64x8 sh[8], perm(_perm);
	for(i = 0; i < 8; ++i)
		for(j = 0; j < 8; ++j)
			sh[i][j] = perm[i] >> encode_shift_ctrl[i * 8 + j];

	// TODO: not needed if vpmultishiftqb is emitted.
	for(i = 0; i < 8; ++i)
		for(j = 0; j < 8; ++j)
			sh[i][j] &= 0x3f;

	u8x64 ret;
	for(i = 0, k = 0; i < 8; ++i)
		for(j = 0; j < 8; ++j)
			ret[k++] = dict[sh[i][j]];

	return ret;
}

//
// Base64 decode
//

/// Decode base64 from in to the buffer at out; out can be 75% of the size
/// of in.
ircd::const_buffer
ircd::b64::decode(const mutable_buffer &out,
                  const string_view &in)
{
	const size_t out_len
	{
		std::min(decode_size(in), size(out))
	};

	size_t i(0), j(0);
	for(; i + 1 <= (size(in) / 64) && i + 1 <= (out_len / 48); ++i)
	{
		// Destination is indexed at 48 byte stride
		const auto di
		{
			reinterpret_cast<u512x1_u *__restrict__>(data(out) + (i * 48))
		};

		// Source is indexed at 64 byte stride
		const auto si
		{
			reinterpret_cast<const u512x1_u *__restrict__>(data(in) + (i * 64))
		};

		*di = decode_block(*si);
	}

	for(; i * 64 < size(in) && i * 48 < out_len; ++i)
	{
		u8x64 block {0};
		for(j = 0; j < 64 && i * 64 + j < size(in); ++j)
			block[j] = in[i * 64 + j];

		block = decode_block(block);
		for(j = 0; j < 48 && i * 48 + j < out_len; ++j)
			out[i * 48 + j] = block[j];
	}

	return string_view
	{
		data(out), out_len
	};
}

/// Decode 64 base64 characters into a 48 byte result. The last 48 bytes of
/// the returned vector are undefined for the caller.
ircd::u8x64
ircd::b64::decode_block(const u8x64 in)
noexcept
{
	size_t i, j;

	i32x16 zz[4];
	for(i = 0; i < 4; ++i)
		for(j = 0; j < 16; ++j)
			zz[i][j] = decode_dictionary[in[i * 16 + j]];

	u8x64 z;
	for(i = 0; i < 4; ++i)
		for(j = 0; j < 16; ++j)
			z[i * 16 + j] = zz[i][j];

	u16x32 al, ah;
	for(i = 0, j = 0; i < 32; ++i)
		ah[i] = z[j++],
		al[i] = z[j++];

	u16x32 a;
	for(i = 0; i < 32; ++i)
		a[i] = ah[i] * 64 + al[i];

	u32x16 bl, bh;
	for(i = 0, j = 0; i < 16; ++i)
		bh[i] = a[j++],
		bl[i] = a[j++];

	u32x16 b;
	for(i = 0; i < 16; ++i)
		b[i] = bh[i] * 4096 + bl[i];

	u8x64 d, c(b);
	for(i = 0; i < 64; ++i)
		d[i] = c[decode_permute_tab_le[i]];

	return d;
}
