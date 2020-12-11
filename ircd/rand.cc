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

ircd::string_view
ircd::rand::string(const mutable_buffer &out,
                   const std::string &dict)
noexcept
{
	assert(!dict.empty());
	std::uniform_int_distribution<size_t> dist
	{
		0, dict.size() - 1
	};

	std::generate(data(out), data(out) + size(out), [&dict, &dist]
	() -> char
	{
		const auto &pos(dist(mt));
		assert(pos < dict.size());
		return dict[pos];
	});

	return out;
}

ircd::const_buffer
ircd::rand::fill(const mutable_buffer &out)
noexcept
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

template<>
ircd::u512x1
ircd::rand::vector()
noexcept
{
	return u64x8
	{
		integer(), integer(),
		integer(), integer(),
		integer(), integer(),
		integer(), integer(),
	};
}

template<>
ircd::u256x1
ircd::rand::vector()
noexcept
{
	return u64x4
	{
		integer(), integer(),
		integer(), integer(),
	};
}

template<>
ircd::u128x1
ircd::rand::vector()
noexcept
{
	return u64x2
	{
		integer(), integer()
	};
}

/// Random integer in range (inclusive)
uint64_t
ircd::rand::integer(const uint64_t &min,
                    const uint64_t &max)
noexcept
{
	std::uniform_int_distribution<uint64_t> dist
	{
		min, max
	};

	return dist(mt);
}

/// Random 64-bits
uint64_t
ircd::rand::integer()
noexcept
{
	return mt();
}
