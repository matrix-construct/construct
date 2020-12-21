// Matrix Construct
//
// Copyright (C) Matrix Construct Developers, Authors & Contributors
// Copyright (C) 2016-2019 Jason Volk <jason@zemos.net>
//
// Permission to use, copy, modify, and/or distribute this software for any
// purpose with or without fee is hereby granted, provided that the above
// copyright notice and this permission notice is present in all copies. The
// full license for this software is available in the LICENSE file.

#include <RB_INC_SYS_TIME_H

decltype(ircd::rfc7231_fmt)
ircd::rfc7231_fmt
{
//	"Load bearing comment"
	"%a, %d %b %Y %T %z"
};

std::ostream &
ircd::operator<<(std::ostream &s, const system_point &tp)
{
	char buf[96];
	return (s << timef(buf, tp));
}

std::ostream &
ircd::operator<<(std::ostream &s, const microtime_t &t)
{
	char buf[64];
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
ircd::microdate(const mutable_buffer &buf)
{
	auto mt
	{
		microtime()
	};

	struct tm lt;
	localtime_r(&mt.first, &lt);
	const auto length
	{
		::snprintf(data(buf), size(buf), "%04d/%02d/%02d %02d:%02d:%02d.%06d",
		           lt.tm_year + 1900,
		           lt.tm_mon + 1,
		           lt.tm_mday,
		           lt.tm_hour,
		           lt.tm_min,
		           lt.tm_sec,
		           mt.second)
	};

	return string_view
	{
		data(buf), size_t(length)
	};
}

ircd::string_view
ircd::ago(const mutable_buffer &buf,
          const system_point &tp,
          const uint &fmt)
{
	const auto diff
	{
		now<system_point>() - tp
	};

	char tmp[64];
	return fmt::sprintf
	{
		buf, "%s ago",
		pretty(tmp, diff, fmt),
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
		duration_cast<seconds>(tse(epoch)).count()
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
		duration_cast<seconds>(tse(epoch)).count()
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

//
// microtime suite
//

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

#if defined(HAVE_GETTIMEOFDAY)
[[gnu::hot]]
ircd::microtime_t
ircd::microtime()
{
	struct timeval tv;
	if(unlikely(gettimeofday(&tv, nullptr) == -1))
	{
		throw_system_error(errno);
		__builtin_unreachable();
	}

	return
	{
		tv.tv_sec, tv.tv_usec
	};
}
#else
[[gnu::hot]]
ircd::microtime_t
ircd::microtime()
{
	const time_t time
	{
		ircd::time<microseconds>()
	};

	const time_t remain
	{
		time % 1'000'000L,
	};

	return
	{
		(time - remain) / 1'000'000, remain
	};
}
#endif
