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

namespace ircd
{
	const char _b64_pad_
	{
		'='
	};

	using _b64_encoder = std::function<string_view (const mutable_buffer &, const const_raw_buffer &)>;
	static std::string _b64encode(const const_raw_buffer &in, const _b64_encoder &);
}

/// Allocate and return a string without padding from the encoding of in
std::string
ircd::b64encode_unpadded(const const_raw_buffer &in)
{
	return _b64encode(in, [](const auto &out, const auto &in)
	{
		return b64encode_unpadded(out, in);
	});
}

/// Allocate and return a string from the encoding of in
std::string
ircd::b64encode(const const_raw_buffer &in)
{
	return _b64encode(in, [](const auto &out, const auto &in)
	{
		return b64encode(out, in);
	});
}

/// Internal; dedupes encoding functions that create and return a string
static std::string
ircd::_b64encode(const const_raw_buffer &in,
                 const _b64_encoder &encoder)
{
	// Allocate a buffer 1.33 times larger than input with pessimistic
	// extra space for any padding and nulling.
	const auto max
	{
		ceil(size(in) * (4.0 / 3.0)) + 4
	};

	std::string ret(max, char{});
	const mutable_buffer buf
	{
		const_cast<char *>(ret.data()), ret.size()
	};

	const string_view encoded
	{
		encoder(buf, in)
	};

	assert(size(encoded) <= ret.size());
	ret.resize(size(encoded));
	return ret;
}

/// Encoding in to base64 at out. Out must be 1.33+ larger than in
/// padding is not present in the returned view.
ircd::string_view
ircd::b64encode(const mutable_buffer &out,
                const const_raw_buffer &in)
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
                         const const_raw_buffer &in)
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
	const mutable_raw_buffer buf
	{
		reinterpret_cast<unsigned char *>(const_cast<char *>(ret.data())), ret.size()
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
ircd::const_raw_buffer
ircd::b64decode(const mutable_raw_buffer &out,
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
