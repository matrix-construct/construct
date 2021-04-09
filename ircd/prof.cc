// Matrix Construct
//
// Copyright (C) Matrix Construct Developers, Authors & Contributors
// Copyright (C) 2016-2019 Jason Volk <jason@zemos.net>
//
// Permission to use, copy, modify, and/or distribute this software for any
// purpose with or without fee is hereby granted, provided that the above
// copyright notice and this permission notice is present in all copies. The
// full license for this software is available in the LICENSE file.

#include <RB_INC_SYS_SYSCALL_H
#include <RB_INC_SYS_IOCTL_H
#include <RB_INC_SYS_MMAN_H
#include <RB_INC_SYS_RESOURCE_H

#include <boost/chrono/chrono.hpp>
#include <boost/chrono/process_cpu_clocks.hpp>

decltype(ircd::prof::log)
ircd::prof::log
{
	"prof"
};

#ifndef __linux__
[[gnu::weak]]
decltype(ircd::prof::psi::supported)
ircd::prof::psi::supported
{
	false
};
#endif

uint64_t
ircd::prof::time_real()
noexcept
{
	return boost::chrono::process_real_cpu_clock::now().time_since_epoch().count();
}

uint64_t
ircd::prof::time_kern()
noexcept
{
	return boost::chrono::process_system_cpu_clock::now().time_since_epoch().count();
}

uint64_t
ircd::prof::time_user()
noexcept
{
	return boost::chrono::process_user_cpu_clock::now().time_since_epoch().count();
}

uint64_t
__attribute__((weak))
ircd::prof::time_thrd()
{
	return 0;
}

uint64_t
__attribute__((weak))
ircd::prof::time_proc()
{
	return 0;
}

///////////////////////////////////////////////////////////////////////////////
//
// prof/vg.h
//
// note: further definitions calling valgrind isolated to ircd/vg.cc

//
// prof::vg::enable
//

ircd::prof::vg::enable::enable()
noexcept
{
	start();
}

ircd::prof::vg::enable::~enable()
noexcept
{
	stop();
}

//
// prof::vg::disable
//

ircd::prof::vg::disable::disable()
noexcept
{
	stop();
}

ircd::prof::vg::disable::~disable()
noexcept
{
	start();
}

///////////////////////////////////////////////////////////////////////////////
//
// prof/syscall_usage_warning.h
//

#ifdef RB_DEBUG
ircd::prof::syscall_usage_warning::~syscall_usage_warning()
noexcept
{
	// Ignore this if we get here during static initialization before main()
	if(unlikely(!ircd::ios::epoch()))
		return;

	const uint64_t total
	{
		timer.stopped?
			timer.at():
			timer.sample()
	};

	if(likely(!total))
		return;

	char buf[256];
	const string_view reason
	{
		fmt::vsprintf
		{
			buf, fmt, ap
		}
	};

	char tmbuf[64];
	log::logf
	{
		log, log::level::DWARNING,
		"[%s] context id:%lu watchdog :system call took %s :%s",
		ctx::current?
			name(ctx::cur()):
		ios::handler::current?
			name(*ios::handler::current):
			"*"_sv,
		ctx::current?
			id(ctx::cur()):
			0,
		pretty(tmbuf, nanoseconds(total), true),
		reason
    };
}
#endif


///////////////////////////////////////////////////////////////////////////////
//
// prof/syscall_timer.h
//

//
// syscall_timer
//

ircd::prof::syscall_timer::syscall_timer()
noexcept
:started
{
	time_kern()
}
,stopped
{
	0UL
}
{
}

uint64_t
ircd::prof::syscall_timer::sample()
{
	stopped = time_kern();
	return at();
}

uint64_t
ircd::prof::syscall_timer::at()
const
{
	return stopped - started;
}

//
// syscall_timer::high_resolution
//

ircd::prof::syscall_timer::high_resolution::high_resolution()
noexcept
:started
{
	resource(prof::sample).at(resource::TIME_KERN)
}
,stopped
{
	0UL
}
{
}

uint64_t
ircd::prof::syscall_timer::high_resolution::sample()
{
	stopped = resource(prof::sample).at(resource::TIME_KERN);
	return at();
}

uint64_t
ircd::prof::syscall_timer::high_resolution::at()
const
{
	return stopped - started;
}

///////////////////////////////////////////////////////////////////////////////
//
// prof/times.h
//

ircd::prof::times::times(sample_t)
:real{}
,kern{}
,user{}
{
	const auto tp
	{
		boost::chrono::process_cpu_clock::now()
	};

	const auto d
	{
		tp.time_since_epoch()
	};

	this->real = d.count().real;
	this->kern = d.count().system;
	this->user = d.count().user;
}

///////////////////////////////////////////////////////////////////////////////
//
// prof/resource.h
//

ircd::prof::resource
ircd::prof::operator-(const resource &a,
                      const resource &b)
{
	resource ret;
	std::transform(begin(a), end(a), begin(b), begin(ret), std::minus<resource::value_type>{});
	return ret;
}

ircd::prof::resource
ircd::prof::operator+(const resource &a,
                      const resource &b)
{
	resource ret;
	std::transform(begin(a), end(a), begin(b), begin(ret), std::plus<resource::value_type>{});
	return ret;
}

ircd::prof::resource &
ircd::prof::operator-=(resource &a,
                       const resource &b)
{
	for(size_t i(0); i < a.size(); ++i)
		a[i] -= b[i];

	return a;
}

ircd::prof::resource &
ircd::prof::operator+=(resource &a,
                       const resource &b)
{
	for(size_t i(0); i < a.size(); ++i)
		a[i] += b[i];

	return a;
}

//
// resource::resource
//

ircd::prof::resource::resource(sample_t)
{
	struct ::rusage ru;
	syscall(::getrusage, RUSAGE_SELF, &ru);

	at(TIME_USER) = ru.ru_utime.tv_sec * 1000000UL + ru.ru_utime.tv_usec;
	at(TIME_KERN) = ru.ru_stime.tv_sec * 1000000UL + ru.ru_stime.tv_usec;
	at(RSS_MAX) = ru.ru_maxrss;
	at(PF_MINOR) = ru.ru_minflt;
	at(PF_MAJOR) = ru.ru_majflt;
	at(BLOCK_IN) = ru.ru_inblock;
	at(BLOCK_OUT) = ru.ru_oublock;
	at(SCHED_YIELD) = ru.ru_nvcsw;
	at(SCHED_PREEMPT) = ru.ru_nivcsw;
}
