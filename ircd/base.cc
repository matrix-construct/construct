// Matrix Construct
//
// Copyright (C) Matrix Construct Developers, Authors & Contributors
// Copyright (C) 2016-2018 Jason Volk <jason@zemos.net>
//
// Permission to use, copy, modify, and/or distribute this software for any
// purpose with or without fee is hereby granted, provided that the above
// copyright notice and this permission notice is present in all copies. The
// full license for this software is available in the LICENSE file.

#include <boost/archive/iterators/base64_from_binary.hpp>
#include <boost/archive/iterators/binary_from_base64.hpp>
#include <boost/archive/iterators/transform_width.hpp>

ircd::string_view
ircd::b64urltob64(const mutable_buffer &out,
                  const string_view &in)
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
	thread_local char tmp[64_KiB];
	if(unlikely(b58decode_size(in) > size(tmp)))
		throw error
		{
			"String too large for conversion at this time."
		};

	return b64encode_unpadded(out, b58decode(tmp, in));
}

ircd::string_view
ircd::b58tob64(const mutable_buffer &out,
               const string_view &in)
{
	thread_local char tmp[64_KiB];
	if(unlikely(b58decode_size(in) > size(tmp)))
		throw error
		{
			"String too large for conversion at this time."
		};

	return b64encode(out, b58decode(tmp, in));
}

ircd::string_view
ircd::b64tob58(const mutable_buffer &out,
               const string_view &in)
{
	thread_local char tmp[64_KiB];
	if(unlikely(b64decode_size(in) > size(tmp)))
		throw error
		{
			"String too large for conversion at this time."
		};

	return b58encode(out, b64decode(tmp, in));
}

namespace ircd
{
	const char _b64_pad_
	{
		'='
	};

	using _b64_encoder = std::function<string_view (const mutable_buffer &, const const_buffer &)>;
	static std::string _b64encode(const const_buffer &in, const _b64_encoder &);
}

/// Allocate and return a string without padding from the encoding of in
std::string
ircd::b64encode_unpadded(const const_buffer &in)
{
	return _b64encode(in, [](const auto &out, const auto &in)
	{
		return b64encode_unpadded(out, in);
	});
}

/// Allocate and return a string from the encoding of in
std::string
ircd::b64encode(const const_buffer &in)
{
	return _b64encode(in, [](const auto &out, const auto &in)
	{
		return b64encode(out, in);
	});
}

/// Internal; dedupes encoding functions that create and return a string
static std::string
ircd::_b64encode(const const_buffer &in,
                 const _b64_encoder &encoder)
{
	// Allocate a buffer 1.33 times larger than input with pessimistic
	// extra space for any padding and nulling.
	const auto max
	{
		ceil(size(in) * (4.0 / 3.0)) + 4
	};

	return string(max, [&in, &encoder]
	(const mutable_buffer &buf)
	{
		return encoder(buf, in);
	});
}

/// Encoding in to base64 at out. Out must be 1.33+ larger than in
/// padding is not present in the returned view.
ircd::string_view
ircd::b64encode(const mutable_buffer &out,
                const const_buffer &in)
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
	memset(data(out) + size(encoded), _b64_pad_, pads);

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
{
	namespace iterators = boost::archive::iterators;
	using transform = iterators::transform_width<unsigned char *, 6, 8>;
	using b64fb = iterators::base64_from_binary<transform>;

	const auto cpsz
	{
		std::min(size(in), size_t(size(out) * (3.0 / 4.0)))
	};

	const auto end
	{
		std::copy(b64fb(data(in)), b64fb(data(in) + cpsz), begin(out))
	};

	const auto len
	{
		size_t(std::distance(begin(out), end))
	};

	assert(len <= size(out));
	return { data(out), len };
}

std::string
ircd::b64decode(const string_view &in)
{
	// Allocate a buffer 75% than input with pessimistic extra space
	const auto max
	{
		ceil(size(in) * 0.75) + 4
	};

	std::string ret(max, char{});
	const mutable_buffer buf
	{
		const_cast<char *>(ret.data()), ret.size()
	};

	const auto decoded
	{
		b64decode(buf, in)
	};

	assert(size(decoded) <= ret.size());
	ret.resize(size(decoded));
	return ret;
}

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
		endswith_count(in, _b64_pad_)
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

namespace ircd
{
	const auto &b58
	{
		"123456789ABCDEFGHJKLMNPQRSTUVWXYZabcdefghijkmnopqrstuvwxyz"s
	};
}

std::string
ircd::b58decode(const string_view &in)
{
	std::string ret(b58decode_size(in), char{});
	const mutable_buffer buf
	{
		const_cast<char *>(ret.data()), ret.size()
	};

	const auto decoded
	{
		b58decode(buf, in)
	};

	assert(size(decoded) <= ret.size());
	ret.resize(size(decoded));
	return ret;
}

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
		auto carry(b58.find(*p));
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

std::string
ircd::b58encode(const const_buffer &in)
{
	return string(b58encode_size(in), [&in]
	(const mutable_buffer &buf)
	{
		return b58encode(buf, in);
	});
}

ircd::string_view
ircd::b58encode(const mutable_buffer &buf,
                const const_buffer &in)
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
			return b58.at(in);
		})
	};
}
