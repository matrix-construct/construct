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
#define HAVE_IRCD_PROF_H

namespace ircd::prof
{
	struct init;
	struct type;
	struct event;
	struct times;
	struct system;
	struct resource;
	struct syscall_timer;
	enum dpl :uint8_t;
	using group = std::vector<std::unique_ptr<event>>;
	IRCD_OVERLOAD(sample)
	IRCD_EXCEPTION(ircd::error, error)

	uint64_t cycles();      ///< Monotonic reference cycles (since system boot)
	uint64_t time_user();   ///< Nanoseconds of CPU time in userspace.
	uint64_t time_kern();   ///< Nanoseconds of CPU time in kernelland.
	uint64_t time_real();   ///< Nanoseconds of CPU time real.
	uint64_t time_proc();   ///< Nanoseconds of CPU time for process.
	uint64_t time_thrd();   ///< Nanoseconds of CPU time for thread.

	system &hotsample(system &) noexcept;
	system &operator+=(system &a, const system &b);
	system &operator-=(system &a, const system &b);
	system operator+(const system &a, const system &b);
	system operator-(const system &a, const system &b);

	resource &operator+=(resource &a, const resource &b);
	resource &operator-=(resource &a, const resource &b);
	resource operator+(const resource &a, const resource &b);
	resource operator-(const resource &a, const resource &b);

	using read_closure = std::function<void (const type &, const uint64_t &val)>;
	void for_each(const const_buffer &read, const read_closure &);

	// Control
	void stop(group &);
	void start(group &);
	void reset(group &);
}

/// X86 platform related
namespace ircd::prof::x86
{
	unsigned long long rdpmc(const uint &);
	unsigned long long rdtscp();
	unsigned long long rdtsc();
}

/// Callgrind hypercall suite
namespace ircd::prof::vg
{
	struct enable;
	struct disable;

	bool enabled();
	void dump(const char *const reason = nullptr);
	void toggle();
	void reset();
	void start() noexcept;
	void stop() noexcept;
}

// Exports to ircd::
namespace ircd
{
	using prof::cycles;
}

/// Enable callgrind profiling for the scope
struct ircd::prof::vg::enable
{
	enable() noexcept;
	~enable() noexcept;
};

/// Disable any enabled callgrind profiling for the scope; then restore.
struct ircd::prof::vg::disable
{
	disable() noexcept;
	~disable() noexcept;
};

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

/// Frontend to times(2). This has low resolution in practice, but it's
/// very cheap as far as syscalls go; x-platform implementation courtesy
/// of boost::chrono.
struct ircd::prof::times
{
	uint64_t real {0};
	uint64_t kern {0};
	uint64_t user {0};

	times(sample_t);
	times() = default;
};

/// Frontend to getrusage(2). This has higher resolution than prof::times
/// in practice with slight added expense.
struct ircd::prof::resource
:std::array<uint64_t, 9>
{
	enum
	{
		TIME_USER, // microseconds
		TIME_KERN, // microseconds
		RSS_MAX,
		PF_MINOR,
		PF_MAJOR,
		BLOCK_IN,
		BLOCK_OUT,
		SCHED_YIELD,
		SCHED_PREEMPT,
	};

	resource(sample_t);
	resource()
	:std::array<uint64_t, 9>{{0}}
	{}
};

/// Frontend to perf_event_open(2). This has the highest resolution.
struct ircd::prof::system
:std::array<std::array<uint64_t, 2>, 7>
{
	using array_type = std::array<std::array<uint64_t, 2>, 7>;

	static prof::group group;

	// [N][0] = KERNEL, [N][1] = USER
	//
	// 0: TIME_PROF,
	// 1: TIME_CPU,
	// 2: TIME_TASK,
	// 3: PF_MINOR,
	// 4: PF_MAJOR,
	// 5: SWITCH_TASK,
	// 6: SWITCH_CPU,

	system(sample_t) noexcept;
	system()
	:array_type{{0}}
	{}
};

/// Type descriptor for prof events. This structure is used to aggregate
/// information that describes a profiling event type, including whether
/// the kernel or the user is being profiled (dpl), the principal counter
/// type being profiled (counter) and any other contextual attributes.
struct ircd::prof::type
{
	enum dpl dpl {0};
	uint8_t type_id {0};
	uint8_t counter {0};
	uint8_t cacheop {0};
	uint8_t cacheres {0};

	type(const event &);
	type(const enum dpl & = (enum dpl)0,
	     const uint8_t &attr_type = 0,
	     const uint8_t &counter = 0,
	     const uint8_t &cacheop = 0,
	     const uint8_t &cacheres = 0);
};

enum ircd::prof::dpl
:std::underlying_type<ircd::prof::dpl>::type
{
	KERNEL  = 0,
	USER    = 1,
};

struct ircd::prof::init
{
	init();
	~init() noexcept;
};

#if defined(__x86_64__) || defined(__i386__)
inline uint64_t
__attribute__((flatten, always_inline, gnu_inline, artificial))
ircd::prof::cycles()
{
	return x86::rdtsc();
}
#else
ircd::prof::cycles()
{
	static_assert(false, "Select reference cycle counter for platform.");
	return 0;
}
#endif

#if defined(__x86_64__) || defined(__i386__)
inline unsigned long long
__attribute__((always_inline, gnu_inline, artificial))
ircd::prof::x86::rdtsc()
{
	return __builtin_ia32_rdtsc();
}
#else
inline unsigned long long
ircd::prof::x86::rdtsc()
{
	return 0;
}
#endif

#if defined(__x86_64__) || defined(__i386__)
inline unsigned long long
__attribute__((always_inline, gnu_inline, artificial))
ircd::prof::x86::rdtscp()
{
	uint32_t ia32_tsc_aux;
	return __builtin_ia32_rdtscp(&ia32_tsc_aux);
}
#else
inline unsigned long long
ircd::prof::x86::rdtscp()
{
	return 0;
}
#endif

#if defined(__x86_64__) || defined(__i386__)
inline unsigned long long
__attribute__((always_inline, gnu_inline, artificial))
ircd::prof::x86::rdpmc(const uint &c)
{
	return __builtin_ia32_rdpmc(c);
}
#else
inline unsigned long long
ircd::prof::x86::rdpmc(const uint &c)
{
	return 0;
}
#endif
