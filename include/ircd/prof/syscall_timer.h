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
#define HAVE_IRCD_PROF_SYSCALL_TIMER_H

namespace ircd::prof
{
	struct syscall_timer;
}

/// This suite of devices is intended to figure out when a system call is
/// really slow or "blocking." The original use-case is for io_submit() in
/// fs::aio.
///
/// The sample is conducted with times(2) which is itself a system call
/// though reasonably fast, and the result has poor resolution meaning
/// the result of at() is generally 0 unless the system call was very slow.
///
/// It is started on construction. The user must later call sample()
/// which returns the value of at() as well.
struct ircd::prof::syscall_timer
{
	struct high_resolution;

	uint64_t started, stopped;

  public:
	uint64_t at() const;
	uint64_t sample();

	syscall_timer() noexcept;
};

/// This is a higher resolution alternative. The sample may be conducted
/// with getrusage() or perf events; the exact method is TBD and may be
/// expensive/intrusive. This device should be used temporarily by developers
/// and not left in place in committed code.
struct ircd::prof::syscall_timer::high_resolution
{
	uint64_t started, stopped;

  public:
	uint64_t at() const;
	uint64_t sample();

	high_resolution() noexcept;
};
