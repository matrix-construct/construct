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

	constexpr size_t byte(const size_t pos) const;
	constexpr size_t bit(const size_t pos) const;

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
	buf[byte(pos)] ^= word(1) << bit(pos);
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
	reset(pos);
	buf[byte(pos)] |= word(val) << bit(pos);
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
	buf[byte(pos)] &= ~(word(1) << bit(pos));
}

template<size_t N>
constexpr size_t
ircd::util::bitset<N>::count()
const
{
	size_t ret(0);
	for(size_t i(0); i < words; ++i)
		ret += popcount(buf[i]);

	return ret;
}

template<size_t N>
constexpr bool
ircd::util::bitset<N>::test(const size_t pos)
const
{
	return buf[byte(pos)] & (word(1) << bit(pos));
}

template<size_t N>
constexpr size_t
ircd::util::bitset<N>::byte(const size_t pos)
const
{
	constexpr auto max(words - 1);
	const auto off(pos / 8);
	if(!__builtin_is_constant_evaluated())
		assert(off <= max);

	return std::min(off, max);
}

template<size_t N>
constexpr size_t
ircd::util::bitset<N>::bit(const size_t pos)
const
{
	return pos % 8;
}
