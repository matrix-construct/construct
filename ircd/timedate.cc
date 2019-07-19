// Matrix Construct
//
// Copyright (C) Matrix Construct Developers, Authors & Contributors
// Copyright (C) 2016-2019 Jason Volk <jason@zemos.net>
//
// Permission to use, copy, modify, and/or distribute this software for any
// purpose with or without fee is hereby granted, provided that the above
// copyright notice and this permission notice is present in all copies. The
// full license for this software is available in the LICENSE file.

decltype(ircd::rfc7231_fmt)
ircd::rfc7231_fmt
{
	"%a, %d %b %Y %T %z"
};

std::ostream &
ircd::operator<<(std::ostream &s, const system_point &tp)
{
	thread_local char buf[96];
	return (s << timef(buf, tp));
}

std::ostream &
ircd::operator<<(std::ostream &s, const microtime_t &t)
{
	thread_local char buf[64];
	s << microtime(buf);
	return s;
}

ircd::string_view
ircd::smalldate(const mutable_buffer &buf,
                const time_t &ltime)
{
	struct tm lt;
	localtime_r(&ltime, &lt);
	const auto len
	{
		::snprintf(data(buf), size(buf), "%04d/%02d/%02d %02d:%02d:%02d",
		           lt.tm_year + 1900,
		           lt.tm_mon + 1,
		           lt.tm_mday,
		           lt.tm_hour,
		           lt.tm_min,
		           lt.tm_sec)
	};

	return
	{
		data(buf), size_t(len)
	};
}

ircd::string_view
ircd::timef(const mutable_buffer &out,
            const char *const &fmt)
{
	const auto epoch
	{
		time()
	};

	return timef(out, epoch, fmt);
}

ircd::string_view
ircd::timef(const mutable_buffer &out,
            localtime_t,
            const char *const &fmt)
{
	const auto epoch
	{
		time()
	};

	return timef(out, epoch, localtime, fmt);
}

ircd::string_view
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

ircd::string_view
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

ircd::string_view
ircd::timef(const mutable_buffer &out,
            const time_t &epoch,
            localtime_t,
            const char *const &fmt)
{
	struct tm tm;
	localtime_r(&epoch, &tm);
	return timef(out, tm, fmt);
}

ircd::string_view
ircd::timef(const mutable_buffer &out,
            const time_t &epoch,
            const char *const &fmt)
{
	struct tm tm;
	gmtime_r(&epoch, &tm);
	return timef(out, tm, fmt);
}

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wformat-nonliteral"
ircd::string_view
ircd::timef(const mutable_buffer &out,
            const struct tm &tm,
            const char *const &fmt)
{
	const auto length
	{
		::strftime(data(out), size(out), fmt, &tm)
	};

	return
	{
		data(out), size_t(length)
	};
}
#pragma GCC diagnostic pop

ircd::string_view
ircd::microtime(const mutable_buffer &buf)
{
	const auto mt
	{
		microtime()
	};

	const auto length
	{
		::snprintf(data(buf), size(buf), "%zd.%06d", mt.first, mt.second)
	};

	return string_view
	{
		data(buf), size_t(length)
	};
}

ircd::microtime_t
ircd::microtime()
{
	struct timeval tv;
	syscall(&::gettimeofday, &tv, nullptr);
	return
	{
		tv.tv_sec, tv.tv_usec
	};
}

template<>
ircd::system_point
ircd::now<ircd::system_point>()
{
	return system_clock::now();
}

template<>
ircd::steady_point
ircd::now<ircd::steady_point>()
{
	return steady_clock::now();
}
