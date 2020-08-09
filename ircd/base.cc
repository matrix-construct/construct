// Matrix Construct
//
// Copyright (C) Matrix Construct Developers, Authors & Contributors
// Copyright (C) 2016-2018 Jason Volk <jason@zemos.net>
//
// Permission to use, copy, modify, and/or distribute this software for any
// purpose with or without fee is hereby granted, provided that the above
// copyright notice and this permission notice is present in all copies. The
// full license for this software is available in the LICENSE file.

#include <boost/archive/iterators/binary_from_base64.hpp>
#include <boost/archive/iterators/transform_width.hpp>

namespace ircd::base
{
	[[gnu::visibility("internal")]]
	thread_local char conv_tmp_buf[64_KiB];
}

//
// Conversion convenience suite
//

ircd::string_view
ircd::b64urltob64(const mutable_buffer &out,
                  const string_view &in)
noexcept
{
	//TODO: optimize with single pass
	string_view ret(in);
	ret = replace(out, ret, '-', '+');
	ret = replace(out, ret, '_', '/');
	return ret;
}

ircd::string_view
ircd::b64tob64url(const mutable_buffer &out,
                  const string_view &in)
noexcept
{
	//TODO: optimize with single pass
	string_view ret(in);
	ret = replace(out, ret, '+', '-');
	ret = replace(out, ret, '/', '_');
	return ret;
}

ircd::string_view
ircd::b58tob64_unpadded(const mutable_buffer &out,
                        const string_view &in)
{
	if(unlikely(b58decode_size(in) > size(base::conv_tmp_buf)))
		throw error
		{
			"String too large for conversion at this time."
		};

	return b64encode_unpadded(out, b58decode(base::conv_tmp_buf, in));
}

ircd::string_view
ircd::b58tob64(const mutable_buffer &out,
               const string_view &in)
{
	if(unlikely(b58decode_size(in) > size(base::conv_tmp_buf)))
		throw error
		{
			"String too large for conversion at this time."
		};

	return b64encode(out, b58decode(base::conv_tmp_buf, in));
}

ircd::string_view
ircd::b64tob58(const mutable_buffer &out,
               const string_view &in)
{
	if(unlikely(b64decode_size(in) > size(base::conv_tmp_buf)))
		throw error
		{
			"String too large for conversion at this time."
		};

	return b58encode(out, b64decode(base::conv_tmp_buf, in));
}

//
// Base64 encode
//

namespace ircd::base
{
	constexpr char _b64_pad_
	{
		'='
	};

	[[using gnu: visibility("internal"), aligned(64)]]
	extern const u8
	b64_encode_permute_tab[64],
	b64_encode_shift_ctrl[64];

	[[using gnu: visibility("internal"), aligned(64)]]
	extern const i32
	b64_encode_lut[64];

	static u8x64 b64encode(const u8x64 in) noexcept;
}

// [00] - [25]  =>  A - Z
// [26] - [51]  =>  a - z
// [52] - [61]  =>  0 - 9
// [62], [63]   =>  +, /
decltype(ircd::base::b64_encode_lut)
ircd::base::b64_encode_lut
{
	'A', 'B', 'C', 'D', 'E', 'F', 'G', 'H',
	'I', 'J', 'K', 'L', 'M', 'N', 'O', 'P',
	'Q', 'R', 'S', 'T', 'U', 'V', 'W', 'X',
	'Y', 'Z', 'a', 'b', 'c', 'd', 'e', 'f',
	'g', 'h', 'i', 'j', 'k', 'l', 'm', 'n',
	'o', 'p', 'q', 'r', 's', 't', 'u', 'v',
	'w', 'x', 'y', 'z', '0', '1', '2', '3',
	'4', '5', '6', '7', '8', '9', '+', '/',
};

/// For vpermb
/// From arXiv:1910.05109v1 [Mula, Lemire] 2 Oct 2019
decltype(ircd::base::b64_encode_permute_tab)
ircd::base::b64_encode_permute_tab
{
	0  + 1,   0  + 0,   0  + 2,   0  + 1,
	3  + 1,   3  + 0,   3  + 2,   3  + 1,
	6  + 1,   6  + 0,   6  + 2,   6  + 1,
	9  + 1,   9  + 0,   9  + 2,   9  + 1,
	12 + 1,   12 + 0,   12 + 2,   12 + 1,
	15 + 1,   15 + 0,   15 + 2,   15 + 1,
	18 + 1,   18 + 0,   18 + 2,   18 + 1,
	21 + 1,   21 + 0,   21 + 2,   21 + 1,
	24 + 1,   24 + 0,   24 + 2,   24 + 1,
	27 + 1,   27 + 0,   27 + 2,   27 + 1,
	30 + 1,   30 + 0,   30 + 2,   30 + 1,
	33 + 1,   33 + 0,   33 + 2,   33 + 1,
	36 + 1,   36 + 0,   36 + 2,   36 + 1,
	39 + 1,   39 + 0,   39 + 2,   39 + 1,
	42 + 1,   42 + 0,   42 + 2,   42 + 1,
	45 + 1,   45 + 0,   45 + 2,   45 + 1,
};

/// For vpmultishiftqb
/// From arXiv:1910.05109v1 [Mula, Lemire] 2 Oct 2019
decltype(ircd::base::b64_encode_shift_ctrl)
ircd::base::b64_encode_shift_ctrl
{
	(10 +  0),  ( 4 +  0),  (22 +  0),  (16 +  0),
	(10 + 32),  ( 4 + 32),  (22 + 32),  (16 + 32),
	(10 +  0),  ( 4 +  0),  (22 +  0),  (16 +  0),
	(10 + 32),  ( 4 + 32),  (22 + 32),  (16 + 32),
	(10 +  0),  ( 4 +  0),  (22 +  0),  (16 +  0),
	(10 + 32),  ( 4 + 32),  (22 + 32),  (16 + 32),
	(10 +  0),  ( 4 +  0),  (22 +  0),  (16 +  0),
	(10 + 32),  ( 4 + 32),  (22 + 32),  (16 + 32),
	(10 +  0),  ( 4 +  0),  (22 +  0),  (16 +  0),
	(10 + 32),  ( 4 + 32),  (22 + 32),  (16 + 32),
	(10 +  0),  ( 4 +  0),  (22 +  0),  (16 +  0),
	(10 + 32),  ( 4 + 32),  (22 + 32),  (16 + 32),
	(10 +  0),  ( 4 +  0),  (22 +  0),  (16 +  0),
	(10 + 32),  ( 4 + 32),  (22 + 32),  (16 + 32),
	(10 +  0),  ( 4 +  0),  (22 +  0),  (16 +  0),
	(10 + 32),  ( 4 + 32),  (22 + 32),  (16 + 32),
};

/// Encoding in to base64 at out. Out must be 1.33+ larger than in
/// padding is not present in the returned view.
ircd::string_view
ircd::b64encode(const mutable_buffer &out,
                const const_buffer &in)
noexcept
{
	const auto pads
	{
		(3 - size(in) % 3) % 3
	};

	const auto encoded
	{
		b64encode_unpadded(out, in)
	};

	assert(size(encoded) + pads <= size(out));
	memset(data(out) + size(encoded), base::_b64_pad_, pads);

	const auto len
	{
		size(encoded) + pads
	};

	return { data(out), len };
}

/// Encoding in to base64 at out. Out must be 1.33+ larger than in.
ircd::string_view
ircd::b64encode_unpadded(const mutable_buffer &out,
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
			reinterpret_cast<u512x1_u *>(data(out) + (i * 64))
		};

		// Source is indexed at 48 byte stride
		const auto si
		{
			reinterpret_cast<const u512x1_u *>(data(in) + (i * 48))
		};

		*di = base::b64encode(*si);
	}

	for(; i * 48 < size(in); ++i)
	{
		u8x64 block{0};
		for(j = 0; i * 48 + j < size(in); ++j)
			block[j] = in[i * 48 + j];

		block = base::b64encode(block);
		for(j = 0; i * 64 + j < out_len; ++j)
			out[i * 64 + j] = block[j];
	}

	return string_view
	{
		data(out), out_len
	};
}

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
ircd::u8x64
ircd::base::b64encode(const u8x64 in)
noexcept
{
	size_t i, j, k;

	// vpermb
	u8x64 _perm;
	for(k = 0; k < 64; ++k)
		_perm[k] = in[b64_encode_permute_tab[k]];

	// TODO: currently does not achieve vpmultshiftqb on avx512vbmi
	u64x8 sh[8], perm(_perm);
	for(i = 0; i < 8; ++i)
		for(j = 0; j < 8; ++j)
			sh[i][j] = perm[i] >> b64_encode_shift_ctrl[i * 8 + j];

	// TODO: not needed if vpmultishiftqb is emitted.
	for(i = 0; i < 8; ++i)
		for(j = 0; j < 8; ++j)
			sh[i][j] &= 0x3f;

	u8x64 ret;
	for(i = 0, k = 0; i < 8; ++i)
		for(j = 0; j < 8; ++j)
			ret[k++] = b64_encode_lut[sh[i][j]];

	return ret;
}

//
// Base64 decode
//

/// Decode base64 from in to the buffer at out; out can be 75% of the size
/// of in.
ircd::const_buffer
ircd::b64decode(const mutable_buffer &out,
                const string_view &in)
{
	namespace iterators = boost::archive::iterators;
	using b64bf = iterators::binary_from_base64<const char *>;
	using transform = iterators::transform_width<b64bf, 8, 6>;

	const auto pads
	{
		endswith_count(in, base::_b64_pad_)
	};

	const auto e
	{
		std::copy(transform(begin(in)), transform(begin(in) + size(in) - pads), begin(out))
	};

	const auto len
	{
		std::distance(begin(out), e)
	};

	assert(size_t(len) <= size(out));
	return { data(out), size_t(len) };
}

//
// Base58
//

namespace ircd::base
{
	static const auto &b58
	{
		"123456789ABCDEFGHJKLMNPQRSTUVWXYZabcdefghijkmnopqrstuvwxyz"s
	};
}

//
// Base58 decode
//

ircd::const_buffer
ircd::b58decode(const mutable_buffer &buf,
                const string_view &in)
{
	auto p(begin(in));
	size_t zeroes(0);
	for(; p != end(in) && *p == '1'; ++p)
		++zeroes;

	const mutable_buffer out
	{
		data(buf) + zeroes, std::min(b58decode_size(in), size(buf) - zeroes)
	};

	assert(size(out) + zeroes <= size(buf));
	memset(data(out), 0, size(out));

	size_t length(0);
	for(size_t i(0); p != end(in); ++p, length = i, i = 0)
	{
		auto carry(base::b58.find(*p));
		if(carry == std::string::npos)
			throw std::out_of_range("Invalid base58 character");

		for(auto it(rbegin(out)); (carry || i < length) && it != rend(out); ++it, i++)
		{
			carry += 58 * (*it);
			*it = carry % 256;
			carry /= 256;
		}
	}

	auto it(begin(buf));
	assert(it + zeroes + length <= end(buf));
	for(; it != end(buf) && zeroes; *it++ = 0, --zeroes);
	memmove(it, data(out) + (size(out) - length), length);
	return { begin(buf), it + length };
}

//
// Base58 encode
//

ircd::string_view
ircd::b58encode(const mutable_buffer &buf,
                const const_buffer &in)
noexcept
{
	auto p(begin(in));
	size_t zeroes(0);
	for(; p != end(in) && *p == 0; ++p)
		++zeroes;

	const mutable_buffer out
	{
		data(buf) + zeroes, std::min(b58encode_size(in), size(buf) - zeroes)
	};

	assert(size(out) + zeroes <= size(buf));
	memset(data(out), 0, size(out));

	size_t length(0);
	for(size_t i(0); p != end(in); ++p, length = i, i = 0)
	{
		size_t carry(*p);
		for(auto it(rbegin(out)); (carry || i < length) && it != rend(out); ++it, i++)
		{
			carry += 256 * (*it);
			*it = carry % 58;
			carry /= 58;
		}
	}

	auto it(begin(buf));
	assert(it + zeroes + length <= end(buf));
	for(; it != end(buf) && zeroes; *it++ = '1', --zeroes);
	memmove(it, data(out) + (size(out) - length), length);
	return
	{
		begin(buf), std::transform(it, it + length, it, []
		(const uint8_t &in)
		{
			return base::b58.at(in);
		})
	};
}
