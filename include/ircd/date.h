/*
 * Copyright (C) 2016 Charybdis Development Team
 * Copyright (C) 2016 Jason Volk <jason@zemos.net>
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice is present in all copies.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING
 * IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 *
 */

#pragma once
#define HAVE_IRCD_UTIL_DATE_H

namespace ircd
{
	using std::chrono::hours;
	using std::chrono::seconds;
	using std::chrono::milliseconds;
	using std::chrono::microseconds;
	using std::chrono::nanoseconds;
	using std::chrono::duration_cast;
	using std::chrono::system_clock;
	using std::chrono::steady_clock;
	using std::chrono::high_resolution_clock;
	using std::chrono::time_point;
	using namespace std::literals::chrono_literals;

	using microtime_t = std::pair<time_t, int32_t>;
	using steady_point = time_point<steady_clock>;
	using system_point = time_point<system_clock>;

	microtime_t microtime();
	string_view microtime(const mutable_buffer &);
	std::ostream &operator<<(std::ostream &, const microtime_t &);

	template<class unit = seconds> unit now();
	template<> steady_point now();
	template<> system_point now();

	template<class unit = seconds> time_t &time(time_t &ref);
	template<class unit = seconds> time_t time();
	template<class unit = seconds> time_t time(time_t *const &ptr);

	const char *const rfc7231_fmt
	{
		"%a, %d %b %Y %T %z"
	};

	IRCD_OVERLOAD(localtime)
	string_view timef(const mutable_buffer &out, const struct tm &tm, const char *const &fmt = rfc7231_fmt);
	string_view timef(const mutable_buffer &out, const time_t &epoch, const char *const &fmt = rfc7231_fmt);
	string_view timef(const mutable_buffer &out, const time_t &epoch, localtime_t, const char *const &fmt = rfc7231_fmt);
	string_view timef(const mutable_buffer &out, localtime_t, const char *const &fmt = rfc7231_fmt);
	string_view timef(const mutable_buffer &out, const char *const &fmt = rfc7231_fmt);
	template<class... args> std::string timestr(args&&...);
}

template<class... args>
std::string
ircd::timestr(args&&... a)
{
	std::string ret(128, char{});
	const mutable_buffer buf
	{
		const_cast<char *>(ret.data()), ret.size()
	};

	ret.resize(timef(buf, std::forward<args>(a)...).size());
	return ret;
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

inline std::ostream &
ircd::operator<<(std::ostream &s, const microtime_t &t)
{
	char buf[64];
	s << microtime(buf);
	return s;
}

inline ircd::string_view
ircd::microtime(const mutable_buffer &buf)
{
	const auto mt{microtime()};
	const auto length
	{
		snprintf(data(buf), size(buf), "%zd.%06d", mt.first, mt.second)
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
	gettimeofday(&tv, nullptr);
	return { tv.tv_sec, tv.tv_usec };
}
