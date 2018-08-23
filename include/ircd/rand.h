// Matrix Construct
//
// Copyright (C) Matrix Construct Developers, Authors & Contributors
// Copyright (C) 2016-2018 Jason Volk <jason@zemos.net>
//
// Permission to use, copy, modify, and/or distribute this software for any
// purpose with or without fee is hereby granted, provided that the above
// copyright notice and this permission notice is present in all copies. The
// full license for this software is available in the LICENSE file.

#pragma once
#define HAVE_IRCD_RAND_H

/// Some character set dictionaries
namespace ircd::rand::dict
{
	extern const std::string alnum;
	extern const std::string alpha;
	extern const std::string upper;
	extern const std::string lower;
	extern const std::string numeric;
}

/// Tools for randomization
namespace ircd::rand
{
	extern std::random_device device;
	extern std::mt19937_64 mt;

	uint64_t integer();
	uint64_t integer(const uint64_t &min, const uint64_t &max); // inclusive

	// Random character from dictionary
	char character(const std::string &dict = dict::alnum);

	// Random string from dictionary
	string_view string(const std::string &dict, const mutable_buffer &out);
	std::string string(const std::string &dict, const size_t &len);
}

// Random character from dictionary
inline char
ircd::rand::character(const std::string &dict)
{
	assert(!dict.empty());
	return dict.at(integer(0, dict.size() - 1));
}

inline uint64_t
ircd::rand::integer(const uint64_t &min,
                    const uint64_t &max)
{
	std::uniform_int_distribution<uint64_t> dist{min, max};
	return dist(mt);
}

inline uint64_t
ircd::rand::integer()
{
	return mt();
}
