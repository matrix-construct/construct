/*
 * Copyright (C) 2017 Charybdis Development Team
 * Copyright (C) 2017 Jason Volk <jason@zemos.net>
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice is present in all copies.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING
 * IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 *
 */

#pragma once
#define HAVE_IRCD_UTIL_RANDOM_H

namespace ircd::rand
{
	extern std::random_device device;
	extern std::mt19937_64 mt;
}

namespace ircd::rand::dict
{
	extern const std::string alnum;
	extern const std::string alpha;
	extern const std::string upper;
	extern const std::string lower;
	extern const std::string numeric;
}

namespace ircd::rand
{
	uint64_t integer();
	uint64_t integer(const uint64_t &min, const uint64_t &max);

	// Random character from dictionary
	char character(const std::string &dict = dict::alnum);

	// Random string of len from dictionary; char buffers null terminated, uchar not.
	template<size_t size> string_view string(const std::string &dict, const size_t &len, char (&buf)[size]);
	string_view string(const std::string &dict, const size_t &len, char *const &buf, const size_t &max);
	string_view string(const std::string &dict, const size_t &len, uint8_t *const &buf);
	std::string string(const std::string &dict, const size_t &len);
}

template<size_t size>
ircd::string_view
ircd::rand::string(const std::string &dict,
                   const size_t &len,
                   char (&buf)[size])
{
	return string(dict, len, buf, size);
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
