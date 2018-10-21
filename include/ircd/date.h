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
#define HAVE_IRCD_DATE_H

namespace ircd
{
	using microtime_t = std::pair<time_t, int32_t>;
	using steady_point = time_point<steady_clock>;
	using system_point = time_point<system_clock>;

	// Standard time_point samples
	template<class unit = seconds> unit now();
	template<> steady_point now();
	template<> system_point now();

	// Standard time_point (system_clock only) directly into long integer.
	template<class unit = seconds> time_t &time(time_t &ref);
	template<class unit = seconds> time_t time();
	template<class unit = seconds> time_t time(time_t *const &ptr);

	// System vdso microtime suite
	microtime_t microtime();
	string_view microtime(const mutable_buffer &);

	// System vdso formatted time suite
	const char *const rfc7231_fmt
	{
		"%a, %d %b %Y %T %z"
	};

	IRCD_OVERLOAD(localtime)
	string_view timef(const mutable_buffer &out, const struct tm &tm, const char *const &fmt = rfc7231_fmt);
	string_view timef(const mutable_buffer &out, const time_t &epoch, const char *const &fmt = rfc7231_fmt);
	string_view timef(const mutable_buffer &out, const time_t &epoch, localtime_t, const char *const &fmt = rfc7231_fmt);
	string_view timef(const mutable_buffer &out, const system_point &epoch, const char *const &fmt = rfc7231_fmt);
	string_view timef(const mutable_buffer &out, const system_point &epoch, localtime_t, const char *const &fmt = rfc7231_fmt);
	string_view timef(const mutable_buffer &out, localtime_t, const char *const &fmt = rfc7231_fmt);
	string_view timef(const mutable_buffer &out, const char *const &fmt = rfc7231_fmt);
	template<size_t max = 128, class... args> std::string timestr(args&&...);

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

inline std::ostream &
ircd::operator<<(std::ostream &s, const system_point &tp)
{
	char buf[96];
	return (s << timef(buf, tp));
}

inline std::ostream &
ircd::operator<<(std::ostream &s, const microtime_t &t)
{
	char buf[64];
	s << microtime(buf);
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

inline ircd::string_view
ircd::timef(const mutable_buffer &out,
            const char *const &fmt)
{
	const auto epoch{time()};
	return timef(out, epoch, fmt);
}

inline ircd::string_view
ircd::timef(const mutable_buffer &out,
            localtime_t,
            const char *const &fmt)
{
	const auto epoch{time()};
	return timef(out, epoch, localtime, fmt);
}

inline ircd::string_view
ircd::timef(const mutable_buffer &out,
            const system_point &epoch,
            localtime_t,
            const char *const &fmt)
{
	const time_t t
	{
		duration_cast<seconds>(epoch.time_since_epoch()).count()
	};

	return timef(out, t, localtime, fmt);
}

inline ircd::string_view
ircd::timef(const mutable_buffer &out,
            const system_point &epoch,
            const char *const &fmt)
{
	const time_t t
	{
		duration_cast<seconds>(epoch.time_since_epoch()).count()
	};

	return timef(out, t, fmt);
}

inline ircd::string_view
ircd::timef(const mutable_buffer &out,
            const time_t &epoch,
            localtime_t,
            const char *const &fmt)
{
	struct tm tm;
	localtime_r(&epoch, &tm);
	return timef(out, tm, fmt);
}

inline ircd::string_view
ircd::timef(const mutable_buffer &out,
            const time_t &epoch,
            const char *const &fmt)
{
	struct tm tm;
	gmtime_r(&epoch, &tm);
	return timef(out, tm, fmt);
}

inline ircd::string_view
ircd::timef(const mutable_buffer &out,
            const struct tm &tm,
            const char *const &fmt)
{
	const auto len
	{
		strftime(data(out), size(out), fmt, &tm)
	};

	return { data(out), len };
}

inline ircd::string_view
ircd::microtime(const mutable_buffer &buf)
{
	const auto mt{microtime()};
	const auto length
	{
		::snprintf(data(buf), size(buf), "%zd.%06d", mt.first, mt.second)
	};

	return string_view
	{
		data(buf), size_t(length)
	};
}

inline ircd::microtime_t
ircd::microtime()
{
	struct timeval tv;
	syscall(&::gettimeofday, &tv, nullptr);
	return { tv.tv_sec, tv.tv_usec };
}

template<class unit>
time_t
ircd::time(time_t *const &ptr)
{
	time_t buf, &ret{ptr? *ptr : buf};
	return time<unit>(ret);
}

template<class unit>
time_t
ircd::time()
{
	time_t ret;
	return time<unit>(ret);
}

template<class unit>
time_t &
ircd::time(time_t &ref)
{
	ref = duration_cast<unit>(system_clock::now().time_since_epoch()).count();
	return ref;
}

template<> inline
ircd::steady_point
ircd::now()
{
	return steady_clock::now();
}

template<> inline
ircd::system_point
ircd::now()
{
	return system_clock::now();
}

template<class unit>
unit
ircd::now()
{
	const auto now
	{
		steady_clock::now()
	};

	const auto tse
	{
		now.time_since_epoch()
	};

	return std::chrono::duration_cast<unit>(tse);
}
