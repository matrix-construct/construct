// Matrix Construct
//
// Copyright (C) Matrix Construct Developers, Authors & Contributors
// Copyright (C) 2016-2022 Jason Volk <jason@zemos.net>
//
// Permission to use, copy, modify, and/or distribute this software for any
// purpose with or without fee is hereby granted, provided that the above
// copyright notice and this permission notice is present in all copies. The
// full license for this software is available in the LICENSE file.

#pragma once
#define HAVE_IRCD_UTIL_BITSET_H

namespace ircd {
inline namespace util
{
	template<size_t>
	struct bitset;
}}

template<size_t N>
struct ircd::util::bitset
{
	using word = uint8_t;

	static constexpr size_t word_bits
	{
		sizeof(word) * 8
	};

	static constexpr size_t words
	{
		N / word_bits?: 1
	};

  private:
	word buf[words] {0};

  public:
	constexpr size_t size() const
	{
		return N;
	}

	constexpr bool test(const size_t pos) const;
	constexpr size_t count() const;

	constexpr void reset(const size_t pos);
	constexpr void reset();
	constexpr void set(const size_t pos, const bool val = true);
	constexpr void set();
	constexpr void flip(const size_t pos);
	constexpr void flip();

	constexpr bitset(const uint128_t val);
	constexpr bitset() = default;
};

template<size_t N>
constexpr
ircd::util::bitset<N>::bitset(const uint128_t val)
{
	const auto bits
	{
		std::min(sizeof(buf) * 8, sizeof(val) * 8)
	};

	size_t i{0};
	for(; i < bits; ++i)
	{
		const decltype(val) one{1};
		set(i, val & (one << i));
	}

	for(; i < size(); ++i)
		set(i, false);
}

template<size_t N>
constexpr void
ircd::util::bitset<N>::flip()
{
	for(size_t i(0); i < words; ++i)
		buf[i] =~ buf[i];
}

template<size_t N>
constexpr void
ircd::util::bitset<N>::flip(const size_t pos)
{
	buf[pos / 8] ^= word(1) << (pos % 8);
}

template<size_t N>
constexpr void
ircd::util::bitset<N>::set()
{
	for(size_t i(0); i < words; ++i)
		buf[i] = ~word{0};
}

template<size_t N>
constexpr void
ircd::util::bitset<N>::set(const size_t pos,
                           const bool val)
{
	buf[pos / 8] |= word(val) << (pos % 8);
}

template<size_t N>
constexpr void
ircd::util::bitset<N>::reset()
{
	for(size_t i(0); i < words; ++i)
		buf[i] = 0;
}

template<size_t N>
constexpr void
ircd::util::bitset<N>::reset(const size_t pos)
{
	buf[pos / 8] &= ~(word(1) << (pos % 8));
}

template<size_t N>
constexpr size_t
ircd::util::bitset<N>::count()
const
{
	size_t ret(0);
	for(size_t i(0); i < words; ++i)
		ret += std::popcount(buf[i]);

	return ret;
}

template<size_t N>
constexpr bool
ircd::util::bitset<N>::test(const size_t pos)
const
{
	return buf[pos / 8] & (word(1) << (pos % 8));
}
