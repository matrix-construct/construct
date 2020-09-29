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

	extern const u8
	encode_permute_tab[64],
	encode_shift_ctrl[64],
	decode_permute_tab[64],
	decode_permute_tab_le[64];

	extern const i32
	decode_tab[256];

	static u8x64 decode_block(const u8x64 block, i64x8 &err) noexcept;

	template<const dictionary &>
	static u8x64 encode_block(const u8x64 block) noexcept;
}
#pragma GCC visibility pop

decltype(ircd::b64::dict_rfc1421)
ircd::b64::dict_rfc1421
alignas(64)
{
	'A', 'B', 'C', 'D', 'E', 'F', 'G', 'H', 'I', 'J', 'K', 'L', 'M', 'N', 'O', 'P',
	'Q', 'R', 'S', 'T', 'U', 'V', 'W', 'X', 'Y', 'Z', 'a', 'b', 'c', 'd', 'e', 'f',
	'g', 'h', 'i', 'j', 'k', 'l', 'm', 'n', 'o', 'p', 'q', 'r', 's', 't', 'u', 'v',
	'w', 'x', 'y', 'z', '0', '1', '2', '3', '4', '5', '6', '7', '8', '9', '+', '/',
};

decltype(ircd::b64::dict_rfc3501)
ircd::b64::dict_rfc3501
alignas(64)
{
	'A', 'B', 'C', 'D', 'E', 'F', 'G', 'H', 'I', 'J', 'K', 'L', 'M', 'N', 'O', 'P',
	'Q', 'R', 'S', 'T', 'U', 'V', 'W', 'X', 'Y', 'Z', 'a', 'b', 'c', 'd', 'e', 'f',
	'g', 'h', 'i', 'j', 'k', 'l', 'm', 'n', 'o', 'p', 'q', 'r', 's', 't', 'u', 'v',
	'w', 'x', 'y', 'z', '0', '1', '2', '3', '4', '5', '6', '7', '8', '9', '+', ',',
};

decltype(ircd::b64::dict_rfc4648)
ircd::b64::dict_rfc4648
alignas(64)
{
	'A', 'B', 'C', 'D', 'E', 'F', 'G', 'H', 'I', 'J', 'K', 'L', 'M', 'N', 'O', 'P',
	'Q', 'R', 'S', 'T', 'U', 'V', 'W', 'X', 'Y', 'Z', 'a', 'b', 'c', 'd', 'e', 'f',
	'g', 'h', 'i', 'j', 'k', 'l', 'm', 'n', 'o', 'p', 'q', 'r', 's', 't', 'u', 'v',
	'w', 'x', 'y', 'z', '0', '1', '2', '3', '4', '5', '6', '7', '8', '9', '-', '_',
};

decltype(ircd::b64::decode_tab)
ircd::b64::decode_tab
alignas(64)
{
	0x40, 0x40, 0x40, 0x40, 0x40, 0x40, 0x40, 0x40, // 7
	0x40, 0x40, 0x40, 0x40, 0x40, 0x40, 0x40, 0x40, // 15
	0x40, 0x40, 0x40, 0x40, 0x40, 0x40, 0x40, 0x40, // 23
	0x40, 0x40, 0x40, 0x40, 0x40, 0x40, 0x40, 0x40, // 31
	0x40, 0x40, 0x40, 0x40, 0x40, 0x40, 0x40, 0x40, // 39
	0x40, 0x40, 0x40,   62,   63,   62, 0x40,   63, // 47
	  52,   53,   54,   55,   56,   57,   58,   59, // 55
	  60,   61, 0x40, 0x40, 0x40, 0x40, 0x40, 0x40, // 63
	0x40,    0,    1,    2,    3,    4,    5,    6, // 71
	   7,    8,    9,   10,   11,   12,   13,   14, // 79
	  15,   16,   17,   18,   19,   20,   21,   22, // 87
	  23,   24,   25, 0x40, 0x40, 0x40, 0x40,   63, // 95
	0x40,   26,   27,   28,   29,   30,   31,   32, // 103
	  33,   34,   35,   36,   37,   38,   39,   40, // 111
	  41,   42,   43,   44,   45,   46,   47,   48, // 119
	  49,   50,   51, 0x40, 0x40, 0x40, 0x40, 0x40, // 127
	0x40, 0x40, 0x40, 0x40, 0x40, 0x40, 0x40, 0x40,
	0x40, 0x40, 0x40, 0x40, 0x40, 0x40, 0x40, 0x40,
	0x40, 0x40, 0x40, 0x40, 0x40, 0x40, 0x40, 0x40,
	0x40, 0x40, 0x40, 0x40, 0x40, 0x40, 0x40, 0x40,
	0x40, 0x40, 0x40, 0x40, 0x40, 0x40, 0x40, 0x40,
	0x40, 0x40, 0x40, 0x40, 0x40, 0x40, 0x40, 0x40,
	0x40, 0x40, 0x40, 0x40, 0x40, 0x40, 0x40, 0x40,
	0x40, 0x40, 0x40, 0x40, 0x40, 0x40, 0x40, 0x40,
	0x40, 0x40, 0x40, 0x40, 0x40, 0x40, 0x40, 0x40,
	0x40, 0x40, 0x40, 0x40, 0x40, 0x40, 0x40, 0x40,
	0x40, 0x40, 0x40, 0x40, 0x40, 0x40, 0x40, 0x40,
	0x40, 0x40, 0x40, 0x40, 0x40, 0x40, 0x40, 0x40,
	0x40, 0x40, 0x40, 0x40, 0x40, 0x40, 0x40, 0x40,
	0x40, 0x40, 0x40, 0x40, 0x40, 0x40, 0x40, 0x40,
	0x40, 0x40, 0x40, 0x40, 0x40, 0x40, 0x40, 0x40,
	0x40, 0x40, 0x40, 0x40, 0x40, 0x40, 0x40, 0x40, // 255
};

decltype(ircd::b64::decode_permute_tab)
ircd::b64::decode_permute_tab
alignas(64)
{
	 6,  0,  1,  2,  9, 10,  4,  5, 12, 13, 14,  8, 22, 16, 17, 18,
	25, 26, 20, 21, 28, 29, 30, 24, 38, 32, 33, 34, 41, 42, 36, 37,
	44, 45, 46, 40, 54, 48, 49, 50, 57, 58, 52, 53, 60, 61, 62, 56,
	 0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
};

/// byte-order swapped for each 32-bit word from above
decltype(ircd::b64::decode_permute_tab_le)
ircd::b64::decode_permute_tab_le
alignas(64)
{
	 2,  1,  0,  6,  5,  4, 10,  9,  8, 14, 13, 12, 18, 17, 16, 22,
	21, 20, 26, 25, 24, 30, 29, 28, 34, 33, 32, 38, 37, 36, 42, 41,
	40, 46, 45, 44, 50, 49, 48, 54, 53, 52, 58, 57, 56, 62, 61, 60,
	 0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
};

/// For vpermb
decltype(ircd::b64::encode_permute_tab)
ircd::b64::encode_permute_tab
alignas(64)
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
alignas(64)
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
template<const ircd::b64::dictionary &dict>
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

	const char _pad[2]
	{
		pads > 0? pad: '\0',
		pads > 1? pad: '\0',
	};

	auto len
	{
		size(encoded)
	};

	len += copy(out + len, _pad[0]) & (pads > 0);
	len += copy(out + len, _pad[1]) & (pads > 1);
	return string_view
	{
		data(out), len
	};
}
template ircd::string_view ircd::b64::encode<ircd::b64::standard>(const mutable_buffer &, const const_buffer &) noexcept;
template ircd::string_view ircd::b64::encode<ircd::b64::urlsafe>(const mutable_buffer &, const const_buffer &) noexcept;

/// Encoding in to base64 at out. Out must be 1.33+ larger than in.
template<const ircd::b64::dictionary &dict>
ircd::string_view
ircd::b64::encode_unpadded(const mutable_buffer &out,
                           const const_buffer &in)
noexcept
{
	char *const __restrict__ dst
	{
		data(out)
	};

	const char *const __restrict__ src
	{
		data(in)
	};

	const size_t res_len
	{
		encode_unpadded_size(in)
	};

	const size_t out_len
	{
		std::min(res_len, size(out))
	};

	u8x64 block {0};
	size_t i(0), j(0);
	for(; i < size(in) / 48 && i < out_len / 64; ++i)
	{
		// Destination is indexed at 64 byte stride
		const auto di
		{
			reinterpret_cast<u512x1_u *__restrict__>(dst  + (i * 64))
		};

		// Source is indexed at 48 byte stride
		const auto si
		{
			reinterpret_cast<const u512x1_u *__restrict__>(src + (i * 48))
		};

		block = *si;
		block = encode_block<dict>(block);
		*di = block;
	}

	for(; i * 48 < size(in) && i * 64 < out_len; ++i)
	{
		#if !defined(__AVX__)
		#pragma clang loop unroll_count(2)
		#endif
		for(j = 0; j < 48 && i * 48 + j < size(in); ++j)
			block[j] = src[i * 48 + j];

		block = encode_block<dict>(block);
		for(j = 0; j < 64 && i * 64 + j < out_len; ++j)
			dst[i * 64 + j] = block[j];
	}

	return string_view
	{
		data(out), out_len
	};
}
template ircd::string_view ircd::b64::encode_unpadded<ircd::b64::dict_rfc1421>(const mutable_buffer &, const const_buffer &) noexcept;
template ircd::string_view ircd::b64::encode_unpadded<ircd::b64::dict_rfc4648>(const mutable_buffer &, const const_buffer &) noexcept;

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
template<const ircd::b64::dictionary &dict>
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

	u32x8 res[8];
	for(i = 0; i < 8; ++i)
		for(j = 0; j < 8; ++j)
			res[i][j] = dict[sh[i][j]];

	u8x64 ret;
	for(i = 0, k = 0; i < 8; ++i)
		for(j = 0; j < 8; ++j)
			ret[k++] = res[i][j];

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
	char *const __restrict__ dst
	{
		data(out)
	};

	const char *const __restrict__ src
	{
		data(in)
	};

	const size_t pads
	{
		endswith_count(in, '=')
	};

	const size_t in_len
	{
		size(in) - pads
	};

	const size_t out_len
	{
		std::min(decode_size(in_len), size(out))
	};

	i64x8 err {0};
	u8x64 block {0};
	size_t i(0), j(0);
	for(; i < in_len / 64 && i < out_len / 48; ++i)
	{
		// Destination is indexed at 48 byte stride
		const auto di
		{
			reinterpret_cast<u512x1_u *__restrict__>(dst + (i * 48))
		};

		// Source is indexed at 64 byte stride
		const auto si
		{
			reinterpret_cast<const u512x1_u *__restrict__>(src + (i * 64))
		};

		block = *si;
		block = decode_block(block, err);
		*di = block;
	}

	for(; i * 64 < in_len && i * 48 < out_len; ++i)
	{
		u8x64 mask {0};
		for(j = 0; j < 64 && i * 64 + j < in_len; ++j)
			block[j] = src[i * 64 + j],
			mask[j] = 0xff;

		i64x8 _err {0};
		block = decode_block(block, _err);
		for(j = 0; j < 48 && i * 48 + j < out_len; ++j)
			dst[i * 48 + j] = block[j];

		err |= _err & i64x8(mask);
	}

	if(unlikely(simd::any(u64x8(err))))
		throw invalid_encoding
		{
			"base64 encoding contained invalid characters."
		};

	return string_view
	{
		data(out), out_len
	};
}

/// Decode 64 base64 characters into a 48 byte result. The last 16 bytes of
/// the returned vector are undefined for the caller.
ircd::u8x64
ircd::b64::decode_block(const u8x64 block,
                        i64x8 &__restrict__ err)
noexcept
{
	size_t i, j;

	i32x16 vals[4];
	for(i = 0; i < 4; ++i)
		for(j = 0; j < 16; ++j)
			vals[i][j] = block[i * 16 + j],
			vals[i][j] = decode_tab[vals[i][j]];

	u8x64 _err;
	i32x16 errs;
	for(i = 0; i < 4; ++i)
		for(j = 0, errs = vals[i] >= 64; j < 16; ++j)
			_err[i * 16 + j] = errs[j];

	u16x32 al, ah;
	for(i = 0; i < 4; ++i)
		for(j = 0; j < 8; ++j)
			ah[i * 8 + j] = vals[i][j * 2 + 0],
			al[i * 8 + j] = vals[i][j * 2 + 1];

	u16x32 a;
	for(i = 0; i < 32; ++i)
		a[i] = ah[i] * 64U + al[i];

	i32x16 b;
	for(i = 0, j = 0; i < 16; ++i, j += 2)
		b[i] = a[j] * 4096U + a[j + 1];

	u8x64 c(b), ret;
	for(i = 0; i < 64; ++i)
		ret[i] = c[decode_permute_tab_le[i]];

	err |= i64x8(_err);
	return ret;
}
