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
#define HAVE_IRCD_UTIL_BSWAP_H

namespace ircd::util
{
	template<class T> T &bswap(T *const &val);
	template<class T> T bswap(const T &val);
}

/// Reverse endian of data pointed to; return reference
template<class T>
T &
ircd::util::bswap(T *const &val)
{
	assert(val != nullptr);
	const auto ptr{reinterpret_cast<uint8_t *>(val)};
	std::reverse(ptr, ptr + sizeof(T));
	return *val;
}

/// Reverse endian of T returning value copy
template<class T>
T
ircd::util::bswap(const T &val)
{
	T ret;
	const auto valptr{reinterpret_cast<const uint8_t *>(&val)};
	const auto retptr{reinterpret_cast<uint8_t *>(&ret)};
	std::reverse_copy(valptr, valptr + sizeof(T), retptr);
	return ret;
}
