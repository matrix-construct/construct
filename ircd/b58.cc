// The Construct
//
// Copyright (C) The Construct Developers, Authors & Contributors
// Copyright (C) 2016-2020 Jason Volk <jason@zemos.net>
//
// Permission to use, copy, modify, and/or distribute this software for any
// purpose with or without fee is hereby granted, provided that the above
// copyright notice and this permission notice is present in all copies. The
// full license for this software is available in the LICENSE file.

namespace ircd::b58
{
	[[gnu::visibility("internal")]]
	extern const string_view dict;

	[[gnu::visibility("internal")]]
	thread_local char conv_tmp_buf[64_KiB];
}

decltype(ircd::b58::dict)
ircd::b58::dict
{
	"123456789ABCDEFGHJKLMNPQRSTUVWXYZabcdefghijkmnopqrstuvwxyz"_sv
};

//
// Conversion convenience suite
//

ircd::string_view
ircd::b58::fromb64(const mutable_buffer &out,
                   const string_view &in)
{
	if(unlikely(b64::decode_size(in) > size(conv_tmp_buf)))
		throw error
		{
			"String too large for conversion at this time."
		};

	return b58::encode(out, b64::decode(conv_tmp_buf, in));
}

ircd::string_view
ircd::b58::tob64_unpadded(const mutable_buffer &out,
                          const string_view &in)
{
	if(unlikely(b58::decode_size(in) > size(conv_tmp_buf)))
		throw error
		{
			"String too large for conversion at this time."
		};

	return b64::encode_unpadded(out, b58::decode(conv_tmp_buf, in));
}

ircd::string_view
ircd::b58::tob64(const mutable_buffer &out,
                  const string_view &in)
{
	if(unlikely(b58::decode_size(in) > size(conv_tmp_buf)))
		throw error
		{
			"String too large for conversion at this time."
		};

	return b64::encode(out, b58::decode(conv_tmp_buf, in));
}

//
// Base58 decode
//

ircd::const_buffer
ircd::b58::decode(const mutable_buffer &buf,
                  const string_view &in)
{
	auto p(begin(in));
	size_t zeroes(0);
	for(; p != end(in) && *p == '1'; ++p)
		++zeroes;

	const mutable_buffer out
	{
		data(buf) + zeroes, std::min(b58::decode_size(in), size(buf) - zeroes)
	};

	assert(size(out) + zeroes <= size(buf));
	memset(data(out), 0, size(out));

	size_t length(0);
	for(size_t i(0); p != end(in); ++p, length = i, i = 0)
	{
		auto carry(dict.find(*p));
		if(unlikely(carry == std::string::npos))
			throw std::out_of_range
			{
				"Invalid base58 character"
			};

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
	return const_buffer
	{
		begin(buf), it + length
	};
}

//
// Base58 encode
//

ircd::string_view
ircd::b58::encode(const mutable_buffer &buf,
                  const const_buffer &in)
noexcept
{
	auto p(begin(in));
	size_t zeroes(0);
	for(; p != end(in) && *p == 0; ++p)
		++zeroes;

	const mutable_buffer out
	{
		data(buf) + zeroes, std::min(b58::encode_size(in), size(buf) - zeroes)
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
	return string_view
	{
		begin(buf), std::transform(it, it + length, it, []
		(const uint8_t &in)
		{
			return dict.at(in);
		})
	};
}
