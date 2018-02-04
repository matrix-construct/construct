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

std::string
ircd::rand::string(const std::string &dict,
                   const size_t &len)
{
	std::string ret(len, char());
	string(dict, len, reinterpret_cast<uint8_t *>(&ret.front()));
	return ret;
}

ircd::string_view
ircd::rand::string(const std::string &dict,
                   const size_t &len,
                   char *const &buf,
                   const size_t &max)
{
	if(unlikely(!max))
		return { buf, max };

	const auto size
	{
		std::min(len, max - 1)
	};

	buf[size] = '\0';
	return string(dict, size, reinterpret_cast<uint8_t *>(buf));
}

ircd::string_view
ircd::rand::string(const std::string &dict,
                   const size_t &len,
                   uint8_t *const &buf)
{
	std::uniform_int_distribution<size_t> dist
	{
		0, dict.size() - 1
	};

	std::generate(buf, buf + len, [&dict, &dist]
	() -> uint8_t
	{
		return dict.at(dist(mt));
	});

	return string_view
	{
		reinterpret_cast<const char *>(buf), len
	};
}
