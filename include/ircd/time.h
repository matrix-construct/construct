// Matrix Construct
//
// Copyright (C) Matrix Construct Developers, Authors & Contributors
// Copyright (C) 2016-2019 Jason Volk <jason@zemos.net>
//
// Permission to use, copy, modify, and/or distribute this software for any
// purpose with or without fee is hereby granted, provided that the above
// copyright notice and this permission notice is present in all copies. The
// full license for this software is available in the LICENSE file.

#pragma once
#define HAVE_IRCD_TIMEDATE_H

namespace ircd
{
	using high_resolution_point = time_point<high_resolution_clock>;
	using steady_point = time_point<steady_clock>;
	using system_point = time_point<system_clock>;
	using microtime_t = std::pair<time_t, int32_t>;
	IRCD_OVERLOAD(localtime)

	// Standard time_point samples
	template<class unit = seconds> unit now();   // monotonic/steady_clock
	template<> steady_point now();               // monotonic/steady_clock
	template<> system_point now();               // system_clock

	// system_clock
	template<class unit = seconds> time_t &time(time_t &ref);
	template<class unit = seconds> time_t time(time_t *const &ptr);
	template<class unit = seconds> time_t time();

	// System microtime suite
	microtime_t microtime();

	// System formatted time suite (also see util/pretty.h)
	extern const char *const rfc7231_fmt;
	string_view timef(const mutable_buffer &out, const struct tm &tm, const char *const &fmt = rfc7231_fmt);
	string_view timef(const mutable_buffer &out, const time_t &epoch, const char *const &fmt = rfc7231_fmt);
	string_view timef(const mutable_buffer &out, const time_t &epoch, localtime_t, const char *const &fmt = rfc7231_fmt);
	string_view timef(const mutable_buffer &out, const system_point &epoch, const char *const &fmt = rfc7231_fmt);
	string_view timef(const mutable_buffer &out, const system_point &epoch, localtime_t, const char *const &fmt = rfc7231_fmt);
	string_view timef(const mutable_buffer &out, localtime_t, const char *const &fmt = rfc7231_fmt);
	string_view timef(const mutable_buffer &out, const char *const &fmt = rfc7231_fmt);
	template<size_t max = 128, class... args> std::string timestr(args&&...);

	// Other tools
	auto tse(const steady_point &);
	auto tse(const system_point &);
	bool operator!(const steady_point &);
	bool operator!(const system_point &);
	string_view ago(const mutable_buffer &buf, const system_point &, const uint &fmt = 0);
	string_view smalldate(const mutable_buffer &buf, const time_t &ltime);
	string_view microdate(const mutable_buffer &buf);
	string_view microtime(const mutable_buffer &);

	// Interface conveniences.
	std::ostream &operator<<(std::ostream &, const microtime_t &);
	std::ostream &operator<<(std::ostream &, const system_point &);
	template<class rep, class period> std::ostream &operator<<(std::ostream &, const duration<rep, period> &);
}

template<class rep,
         class period>
std::ostream &
ircd::operator<<(std::ostream &s,
                 const duration<rep, period> &duration)
{
	s << duration.count();
	return s;
}

/// timestr() is a passthru to timef() where you don't give the first argument
/// (the mutable_buffer). Instead of supplying a buffer an allocated
/// std::string is returned with the result. By default this string's buffer
/// is sufficiently large, but may be further tuned in the template parameter.
template<size_t max,
         class... args>
std::string
ircd::timestr(args&&... a)
{
	return string(max, [&](const mutable_buffer &buf)
	{
		return timef(buf, std::forward<args>(a)...);
	});
}

//
// inline tools
//

extern inline auto
__attribute__((always_inline, gnu_inline, artificial, flatten))
ircd::tse(const system_point &sp)
{
	return sp.time_since_epoch();
}

extern inline auto
__attribute__((always_inline, gnu_inline, artificial, flatten))
ircd::tse(const steady_point &sp)
{
	return sp.time_since_epoch();
}

extern inline bool
__attribute__((always_inline, gnu_inline, artificial, flatten))
ircd::operator!(const system_point &sp)
{
	return !tse(sp).count();
}

extern inline bool
__attribute__((always_inline, gnu_inline, artificial, flatten))
ircd::operator!(const steady_point &sp)
{
	return !tse(sp).count();
}

//
// system_clock
//

template<class unit>
extern inline time_t
__attribute__((always_inline, gnu_inline, artificial, flatten))
ircd::time()
{
	time_t ret;
	return time<unit>(ret);
}

template<class unit>
extern inline time_t
__attribute__((always_inline, gnu_inline, artificial, flatten))
ircd::time(time_t *const &ptr)
{
	time_t buf, &ret{ptr? *ptr : buf};
	return time<unit>(ret);
}

template<class unit>
extern inline time_t &
__attribute__((always_inline, gnu_inline, artificial, flatten))
ircd::time(time_t &ref)
{
	const auto &now
	{
		tse(ircd::now<system_point>())
	};

	ref = duration_cast<unit>(now).count();
	return ref;
}

template<>
inline ircd::system_point
__attribute__((always_inline, artificial, flatten))
ircd::now<ircd::system_point>()
{
    return system_clock::now();
}

//
// steady_clock
//

template<class unit>
extern inline unit
__attribute__((always_inline, gnu_inline, artificial, flatten))
ircd::now()
{
	const auto &now
	{
		tse(ircd::now<steady_point>())
	};

	return std::chrono::duration_cast<unit>(now);
}

template<>
inline ircd::steady_point
__attribute__((always_inline, artificial, flatten))
ircd::now<ircd::steady_point>()
{
    return steady_clock::now();
}
