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
inline namespace util {

inline std::stringbuf &
pubsetbuf(std::stringbuf &sb,
          const mutable_buffer &buf)
{
	sb.pubsetbuf(data(buf), size(buf));
	return sb;
}

template<class stringstream>
inline stringstream &
pubsetbuf(stringstream &ss,
          const mutable_buffer &buf)
{
	ss.rdbuf()->pubsetbuf(data(buf), size(buf));
	return ss;
}

template<class stringstream>
inline stringstream &
pubsetbuf(stringstream &ss,
          std::string &s)
{
	auto *const &data
	{
		mutable_cast(s.data())
	};

	ss.rdbuf()->pubsetbuf(data, s.size());
	return ss;
}

template<class stringstream>
inline stringstream &
pubsetbuf(stringstream &ss,
          std::string &s,
          const size_t &size)
{
	s.resize(size, char{});
	return pubsetbuf(ss, s);
}

template<class stringstream>
inline stringstream &
resizebuf(stringstream &ss,
          std::string &s)
{
	const auto pos(ss.tellp());
	assert(pos >= 0);
	s.resize(pos);
	return ss;
}

#if RB_CXX_EPOCH >= 11 || defined(__clang__)
/// since c++20
inline string_view
view(std::stringbuf &sb,
     const const_buffer &buf = {})
{
	const string_view ret
	{
		sb.view()
	};

	assert(!buf || data(ret) == data(buf));
	return ret;
}
#endif

/// buf has to match the rdbuf you gave the stringstream
template<class stringstream>
inline string_view
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
