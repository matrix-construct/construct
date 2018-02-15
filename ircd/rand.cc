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
	return ircd::util::string(len, [&dict]
	(const mutable_buffer &buf)
	{
		return string(dict, buf);
	});
}

ircd::string_view
ircd::rand::string(const std::string &dict,
                   const mutable_buffer &out)
{
	std::uniform_int_distribution<size_t> dist
	{
		0, dict.size() - 1
	};

	std::generate(data(out), data(out) + size(out), [&dict, &dist]
	() -> char
	{
		return dict.at(dist(mt));
	});

	return out;
}
