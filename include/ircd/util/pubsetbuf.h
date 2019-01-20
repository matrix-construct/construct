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
#define HAVE_IRCD_UTIL_PUBSETBUF_H

//
// stringstream buffer set macros
//

namespace ircd {
namespace util {

template<class stringstream>
stringstream &
pubsetbuf(stringstream &ss,
          const mutable_buffer &buf)
{
	ss.rdbuf()->pubsetbuf(data(buf), size(buf));
	return ss;
}

template<class stringstream>
stringstream &
pubsetbuf(stringstream &ss,
          std::string &s)
{
	auto *const &data
	{
		const_cast<char *>(s.data())
	};

	ss.rdbuf()->pubsetbuf(data, s.size());
	return ss;
}

template<class stringstream>
stringstream &
pubsetbuf(stringstream &ss,
          std::string &s,
          const size_t &size)
{
	s.resize(size, char{});
	return pubsetbuf(ss, s);
}

template<class stringstream>
stringstream &
resizebuf(stringstream &ss,
          std::string &s)
{
	s.resize(ss.tellp());
	return ss;
}

/// buf has to match the rdbuf you gave the stringstream
template<class stringstream>
string_view
view(stringstream &ss,
     const const_buffer &buf)
{
	const auto tell
	{
		std::min(size_t(ss.tellp()), size(buf))
	};

	ss.flush();
	ss.rdbuf()->pubsync();
	const string_view ret
	{
		data(buf), tell
	};

	assert(size(ret) <= size(buf));
	return ret;
}

} // namespace util
} // namespace ircd
