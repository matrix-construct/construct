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

namespace ircd {
inline namespace util
{
	template<class T> T &bswap(T *const &);
	template<class T> T bswap(T);

	// Host <-> network endian
	template<class T> T &hton(T *const &);
	template<class T> T &ntoh(T *const &);
	template<class T> T hton(T);
	template<class T> T ntoh(T);

	// Convenience host to network interface for arpa/inet.h compat
	uint32_t htonl(const uint32_t &);
	uint16_t htons(const uint16_t &);
	double htond(const double &);
	float htonf(const float &);

	// Convenience network to host interface for arpa/inet.h compat
	uint32_t ntohl(const uint32_t &);
	uint16_t ntohs(const uint16_t &);
	double ntohd(const double &);
	float ntohf(const float &);
}}

inline float
ircd::util::ntohf(const float &a)
{
	return ntoh(a);
}

inline double
ircd::util::ntohd(const double &a)
{
	return ntoh(a);
}

inline uint32_t
ircd::util::ntohl(const uint32_t &a)
{
	return ntoh(a);
}

inline uint16_t
ircd::util::ntohs(const uint16_t &a)
{
	return ntoh(a);
}

inline float
ircd::util::htonf(const float &a)
{
	return hton(a);
}

inline double
ircd::util::htond(const double &a)
{
	return hton(a);
}

inline uint32_t
ircd::util::htonl(const uint32_t &a)
{
	return hton(a);
}

inline uint16_t
ircd::util::htons(const uint16_t &a)
{
	return hton(a);
}

template<class T>
T
ircd::util::hton(T a)
{
	return hton<T>(&a);
}

template<class T>
T
ircd::util::ntoh(T a)
{
	return ntoh<T>(&a);
}

template<class T>
T &
ircd::util::hton(T *const &a)
{
	#if __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
		return bswap(a);
	#else
		assert(a);
		return *a;
	#endif
}

template<class T>
T &
ircd::util::ntoh(T *const &a)
{
	#if __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
		return bswap(a);
	#else
		assert(a);
		return *a;
	#endif
}

/// Reverse endian of T returning value copy
template<class T>
T
ircd::util::bswap(T ret)
{
	return bswap(&ret);
}

/// Reverse endian of data pointed to; return reference
template<class T>
T &
ircd::util::bswap(T *const &val)
{
	assert(val != nullptr);
	const auto &ptr
	{
		reinterpret_cast<uint8_t *const &>(val)
	};

	std::reverse(ptr, ptr + sizeof(T));
	return *val;
}
