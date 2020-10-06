// Matrix Construct
//
// Copyright (C) Matrix Construct Developers, Authors & Contributors
// Copyright (C) 2016-2018 Jason Volk <jason@zemos.net>
//
// Permission to use, copy, modify, and/or distribute this software for any
// purpose with or without fee is hereby granted, provided that the above
// copyright notice and this permission notice is present in all copies. The
// full license for this software is available in the LICENSE file.

decltype(ircd::rand::device) ircd::rand::device
{
	// on linux: uses RDRND or /dev/urandom
	// on windows: TODO: verify construction source
};

decltype(ircd::rand::mt) ircd::rand::mt
{
	device()
};

decltype(ircd::rand::dict::alnum) ircd::rand::dict::alnum
{
	"0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz"
};

decltype(ircd::rand::dict::alpha) ircd::rand::dict::alpha
{
	"ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz"
};

decltype(ircd::rand::dict::upper) ircd::rand::dict::upper
{
	"ABCDEFGHIJKLMNOPQRSTUVWXYZ"
};

decltype(ircd::rand::dict::lower) ircd::rand::dict::lower
{
	"abcdefghijklmnopqrstuvwxyz"
};

decltype(ircd::rand::dict::numeric) ircd::rand::dict::numeric
{
	"0123456789"
};

ircd::const_buffer
ircd::rand::fill(const mutable_buffer &out)
{
	uint64_t *const __restrict__ val
	{
		reinterpret_cast<uint64_t *>(data(out))
	};

	constexpr size_t word_size
	{
		sizeof(*val)
	};

	size_t i(0);
	for(; i < size(out) / word_size; ++i)
		val[i] = integer();

	for(size_t j(i * word_size); j < size(out) % word_size; ++j)
		out[j] = integer();

	return out;
}

ircd::string_view
ircd::rand::string(const mutable_buffer &out,
                   const std::string &dict)
{
	std::uniform_int_distribution<size_t> dist
	{
		0, dict.size() - 1
	};

	std::generate(data(out), data(out) + size(out), [&dict, &dist]
	{
		return char(dict.at(dist(mt)));
	});

	return out;
}
