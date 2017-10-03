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
	ssize_t microtime(char *const &buf, const size_t &size);
	std::ostream &operator<<(std::ostream &, const microtime_t &);

	template<class unit = seconds> unit now();
	template<> steady_point now();
	template<> system_point now();

	template<class unit = seconds> time_t &time(time_t &ref);
	template<class unit = seconds> time_t time();
	template<class unit = seconds> time_t time(time_t *const &ptr);
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
	const size_t len(microtime(buf, sizeof(buf)));
	s << string_view{buf, len};
	return s;
}

inline ssize_t
ircd::microtime(char *const &buf,
                const size_t &size)
{
	const auto mt(microtime());
	return snprintf(buf, size, "%zd.%06d", mt.first, mt.second);
}

inline ircd::microtime_t
ircd::microtime()
{
	struct timeval tv;
	gettimeofday(&tv, nullptr);
	return { tv.tv_sec, tv.tv_usec };
}
