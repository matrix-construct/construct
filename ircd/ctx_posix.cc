// The Construct
//
// Copyright (C) The Construct Developers, Authors & Contributors
// Copyright (C) 2016-2020 Jason Volk <jason@zemos.net>
//
// Permission to use, copy, modify, and/or distribute this software for any
// purpose with or without fee is hereby granted, provided that the above
// copyright notice and this permission notice is present in all copies. The
// full license for this software is available in the LICENSE file.

///////////////////////////////////////////////////////////////////////////////
//
// This unit exists to mitigate unwanted use of pthreads by third-party
// libraries. It is NOT intended to supplant real threads with ircd::ctx at
// this time, as we still want real parallel execution ability available to
// the project and to other users of the address space.
//

#include <dlfcn.h>
#include <pthread.h>
#include "ctx_posix.h"

//#define IRCD_PTHREAD_DEADLK_CHK

namespace ircd::ctx::posix
{
	static bool is_main_thread() noexcept;
	static bool hook_enabled() noexcept;
	static bool is(const pthread_t &) noexcept;

	[[gnu::visibility("internal")]]
	extern void *real_pthread; // custom_ptr<void> real_pthread;
}

using ircd::always_assert;

/// Unit's logging facility.
decltype(ircd::ctx::posix::log)
ircd::ctx::posix::log
{
	"ctx.posix"
};

/// Points to a dlopen(3) handle of libpthread.so to give us the untainted
/// location of real pthread functions; regardless of our hook solution for
/// the platform. It remains null until this interface is used to spawn an
/// actually real pthread. Note that this might not be available if static
/// destruction occurs prior to any joining thread accessing it.
decltype(ircd::ctx::posix::real_pthread)
ircd::ctx::posix::real_pthread
{
	nullptr, //::dlclose
};

/// -1 = pthread interface not hooked, forwards to real pthread.
///  0 = determined automatically based on contextual information.
///  1 = pthread interface hooked, forwards to ircd::ctx.
decltype(ircd::ctx::posix::enable_hook)
ircd::ctx::posix::enable_hook;

/// State container for ircd::ctx's that are being operated through the hooked
/// pthread interface.
decltype(ircd::ctx::posix::ctxs)
ircd::ctx::posix::ctxs;

/// Hook generation macro. If the hook is accomplished with __wrap/__real
/// then also be sure to add the line to the linker flags in Makefile.am.
#define IRCD_WRAP(symbol, target, prototype, body)                         \
    extern "C" int __real_##symbol prototype;                              \
    extern "C" int __wrap_##symbol prototype noexcept body                 \
    extern "C" int symbol prototype __attribute__((weak, alias(target)));

///////////////////////////////////////////////////////////////////////////////
//
// pthread supplement
//

IRCD_WRAP(pthread_create, "__wrap_pthread_create",
(
	pthread_t *const thread,
	const pthread_attr_t *const attr,
	void *(*const start_routine)(void *),
	void *const arg
),
{
	if(ircd::ctx::posix::hook_enabled())
		return ircd_pthread_create(thread, attr, start_routine, arg);

	// hack on the hack: linker's __real_pthread_create isn't working and we
	// need something stronger.
	if(!ircd::ctx::posix::real_pthread)
	{
		// 2p the dlopen() handle so it's init once by any real thread.
		static std::mutex mutex;
		const std::lock_guard lock
		{
			mutex
		};

		if(!ircd::ctx::posix::real_pthread)
			ircd::ctx::posix::real_pthread = //.reset
			(
				dlopen("libpthread.so", RTLD_LOCAL | RTLD_LAZY)
			);
	}

	assert(ircd::ctx::posix::real_pthread);
	const auto __real_pthread_create
	{
		reinterpret_cast<int (*)(pthread_t *, const pthread_attr_t *, void *(*)(void *), void *)>
		(
			dlsym(ircd::ctx::posix::real_pthread, "pthread_create")
		)
	};

	return __real_pthread_create(thread, attr, start_routine, arg);
})

int
ircd_pthread_create(pthread_t *const thread,
                    const pthread_attr_t *const attr,
                    void *(*const start_routine)(void *),
                    void *const arg)
noexcept
{
	assert(thread);
	assert(start_routine);

	ircd::ctx::posix::ctxs.emplace_back(ircd::context
	{
		"pthread",
		1024 * 1024 * 1UL,
		ircd::context::POST,
		std::bind(start_routine, arg),
	});

	*thread = id(ircd::ctx::posix::ctxs.back());

	ircd::log::logf
	{
		ircd::ctx::posix::log, ircd::log::level::DEBUG,
		"pthread_create id:%lu attr:%p func:%p arg:%p",
		*thread,
		attr,
		start_routine,
		arg
	};

	return 0;
}

IRCD_WRAP(pthread_join, "__wrap_pthread_join",
(
	pthread_t __th,
	void **__thread_return
),
{
	if(ircd::ctx::posix::is(__th))
		return ircd_pthread_join(__th, __thread_return);

	assert(ircd::ctx::posix::real_pthread);
	const auto __real_pthread_join
	{
		reinterpret_cast<int (*)(pthread_t, void **)>
		(
			dlsym(ircd::ctx::posix::real_pthread, "pthread_join")
		)
	};

	assert(__real_pthread_join);
	return __real_pthread_join(__th, __thread_return);
})

int
ircd_pthread_join(pthread_t __th,
                  void **__thread_return)
noexcept
{
	ircd::log::debug
	{
		ircd::ctx::posix::log, "pthread_join id:%lu thread_return:%p",
		__th,
		__thread_return,
	};

	auto it(begin(ircd::ctx::posix::ctxs));
	while(it != end(ircd::ctx::posix::ctxs))
	{
		if(id(*it) == __th)
		{
			it->join();
			it = ircd::ctx::posix::ctxs.erase(it);
			break;
		}
		else ++it;
	}

	if(__thread_return)
		*__thread_return = PTHREAD_CANCELED;

	return 0;
}

IRCD_WRAP(pthread_tryjoin_np, "__wrap_pthread_tryjoin_np",
(
	pthread_t __th,
	void **__thread_return
),
{
	if(ircd::ctx::posix::is(__th))
		return ircd_pthread_tryjoin_np(__th, __thread_return);

	assert(ircd::ctx::posix::real_pthread);
	const auto __real_pthread_tryjoin_np
	{
		reinterpret_cast<int (*)(pthread_t, void **)>
		(
			dlsym(ircd::ctx::posix::real_pthread, "pthread_tryjoin_np")
		)
	};

	return __real_pthread_tryjoin_np(__th, __thread_return);
})

int
ircd_pthread_tryjoin_np(pthread_t __th,
                        void **__thread_return)
noexcept
{
	always_assert(false);
	return EINVAL;
}

IRCD_WRAP(pthread_timedjoin_np, "__wrap_pthread_timedjoin_np",
(
	pthread_t __th,
	void **__thread_return,
	const struct timespec *__abstime
),
{
	if(ircd::ctx::posix::is(__th))
		return ircd_pthread_timedjoin_np(__th, __thread_return, __abstime);

	assert(ircd::ctx::posix::real_pthread);
	const auto __real_pthread_timedjoin_np
	{
		reinterpret_cast<int (*)(pthread_t, void **, const struct timespec *)>
		(
			dlsym(ircd::ctx::posix::real_pthread, "pthread_timedjoin_np")
		)
	};

	assert(__real_pthread_timedjoin_np);
	return __real_pthread_timedjoin_np(__th, __thread_return, __abstime);
})

int
ircd_pthread_timedjoin_np(pthread_t __th,
                          void **__thread_return,
                          const struct timespec *__abstime)
noexcept
{
	//TODO: XXX ctx timed join
	ircd_pthread_join(__th, __thread_return);
	return 0;
}

IRCD_WRAP(pthread_clockjoin_np, "__wrap_pthread_clockjoin_np",
(
	pthread_t __th,
	void **__thread_return,
	clockid_t clockid,
	const struct timespec *__abstime
),
{
	if(ircd::ctx::posix::is(__th))
		return ircd_pthread_clockjoin_np(__th, __thread_return, clockid, __abstime);

	assert(ircd::ctx::posix::real_pthread);
	const auto __real_pthread_clockjoin_np
	{
		reinterpret_cast<int (*)(pthread_t, void **, clockid_t, const struct timespec *)>
		(
			dlsym(ircd::ctx::posix::real_pthread, "pthread_clockjoin_np")
		)
	};

	assert(__real_pthread_clockjoin_np);
	return __real_pthread_clockjoin_np(__th, __thread_return, clockid, __abstime);
})

int
ircd_pthread_clockjoin_np(pthread_t __th,
                          void **__thread_return,
                          clockid_t clockid,
                          const struct timespec *__abstime)
noexcept
{
	//TODO: XXX ctx clock join
	ircd_pthread_join(__th, __thread_return);
	return 0;
}

void
__attribute__((noreturn))
ircd_pthread_exit(void *const retval)
noexcept
{
	always_assert(false);
	__builtin_unreachable();
}

int
ircd_pthread_detach(pthread_t __th)
noexcept
{
	always_assert(false);
	return EINVAL;
}

extern "C" pthread_t
__real_pthread_self(void);

extern "C" pthread_t
__wrap_pthread_self(void)
{
	const bool hook_enabled
	{
		true
		&& ircd::ctx::current
		&& ircd::ctx::posix::enable_hook >= 0
	};

	if(hook_enabled)
		return ircd_pthread_self();

	return __real_pthread_self();
}

#if 0
extern "C" pthread_t
pthread_self(void)
__attribute__((weak, alias("__wrap_pthread_self")));
#endif

pthread_t
ircd_pthread_self(void)
noexcept
{
	assert(ircd::ctx::current);
	return id(ircd::ctx::cur());
}

int
ircd_pthread_getcpuclockid(pthread_t __thread_id,
                          __clockid_t *__clock_id)
noexcept
{
	always_assert(false);
	return EINVAL;
}

int
ircd_pthread_atfork(void (*__prepare)(void),
                    void (*__parent)(void),
                    void (*__child)(void))
noexcept
{
	always_assert(false);
	return EINVAL;
}

//
// Initialization
//

int
ircd_pthread_once(pthread_once_t *__once_control,
                  void (*__init_routine)(void))
noexcept
{
	static_assert(sizeof(std::atomic<int>) == sizeof(pthread_once_t));

	auto *const _once_control
	{
		reinterpret_cast<std::atomic<int> *>(__once_control)
	};

	const int once_control
	{
		std::atomic_exchange(_once_control, 1)
	};

	assert(once_control == 1 || once_control == 0);
	if(likely(once_control == 0))
		__init_routine();

	return 0;
}

//
// Cancellation
//

int
ircd_pthread_setcancelstate(int __state,
                            int *__oldstate)
noexcept
{
	always_assert(false);
	return EINVAL;
}

int
ircd_pthread_setcanceltype(int __type,
                           int *__oldtype)
noexcept
{
	always_assert(false);
	return EINVAL;
}

int
ircd_pthread_cancel(pthread_t __th)
noexcept
{
	always_assert(false);
	return EINVAL;
}

void
ircd_pthread_testcancel(void)
noexcept
{
	always_assert(false);
}

//
// Scheduling
//

int
ircd_pthread_setschedparam(pthread_t __target_thread,
                           int __policy,
                           const struct sched_param *__param)
noexcept
{
	always_assert(false);
	return EINVAL;
}

int
ircd_pthread_getschedparam(pthread_t __target_thread,
                           int *__restrict __policy,
                           struct sched_param *__restrict __param)
noexcept
{
	always_assert(false);
	return EINVAL;
}

int
ircd_pthread_setschedprio(pthread_t __target_thread,
                          int __prio)
noexcept
{
	always_assert(false);
	return EINVAL;
}

int
ircd_pthread_getname_np(pthread_t __target_thread,
                        char *__buf,
                        size_t __buflen)
noexcept
{
	always_assert(false);
	return EINVAL;
}

IRCD_WRAP(pthread_setname_np, "__wrap_pthread_setname_np",
(
	pthread_t __target_thread,
	const char *__name
),
{
	if(ircd::ctx::posix::is(__target_thread))
		return ircd_pthread_setname_np(__target_thread, __name);

	assert(ircd::ctx::posix::real_pthread);
	const auto __real_pthread_setname_np
	{
		reinterpret_cast<int (*)(pthread_t, const char *)>
		(
			dlsym(ircd::ctx::posix::real_pthread, "pthread_setname_np")
		)
	};

	assert(__real_pthread_setname_np);
	return __real_pthread_setname_np(__target_thread, __name);
})

int
ircd_pthread_setname_np(pthread_t __target_thread,
                        const char *__name)
noexcept
{
	auto it(begin(ircd::ctx::posix::ctxs));
	while(it != end(ircd::ctx::posix::ctxs))
	{
		if(id(*it) == __target_thread)
		{
			name(*it, __name);
			break;
		}
		else ++it;
	}

	return 0;
}

int
ircd_pthread_getconcurrency(void)
noexcept
{
	always_assert(false);
	return EINVAL;
}

int
ircd_pthread_setconcurrency(int __level)
noexcept
{
	always_assert(false);
	return EINVAL;
}

int
ircd_pthread_setaffinity_np(pthread_t __th,
                            size_t __cpusetsize,
                            const cpu_set_t *__cpuset)
noexcept
{
	always_assert(false);
	return EINVAL;
}

int
ircd_pthread_getaffinity_np(pthread_t __th,
                            size_t __cpusetsize,
                            cpu_set_t *__cpuset)
noexcept
{
	always_assert(false);
	return EINVAL;
}

int
ircd_pthread_yield(void)
noexcept
{
	assert(ircd::ctx::current);
	ircd::ctx::yield();
	return 0;
}

//
// Attributes
//

int
ircd_pthread_attr_init(pthread_attr_t *__attr)
noexcept
{
	always_assert(false);
	return EINVAL;
}

int
ircd_pthread_attr_destroy(pthread_attr_t *__attr)
noexcept
{
	always_assert(false);
	return EINVAL;
}

int
ircd_pthread_attr_getdetachstate(const pthread_attr_t *__attr,
                                 int *__detachstate)
noexcept
{
	always_assert(false);
	return EINVAL;
}

int
ircd_pthread_attr_setdetachstate(pthread_attr_t *__attr,
                                 int __detachstate)
noexcept
{
	always_assert(false);
	return EINVAL;
}

int
ircd_pthread_attr_getguardsize(const pthread_attr_t *__attr,
                               size_t *__guardsize)
noexcept
{
	always_assert(false);
	return EINVAL;
}

int
ircd_pthread_attr_setguardsize(pthread_attr_t *__attr,
                               size_t __guardsize)
noexcept
{
	always_assert(false);
	return EINVAL;
}

int
ircd_pthread_attr_getschedparam(const pthread_attr_t *__restrict __attr,
                                struct sched_param *__restrict __param)
noexcept
{
	always_assert(false);
	return EINVAL;
}

int
ircd_pthread_attr_setschedparam(pthread_attr_t *__restrict __attr,
                                const struct sched_param *__restrict __param)
noexcept
{
	always_assert(false);
	return EINVAL;
}

int
ircd_pthread_attr_getschedpolicy(const pthread_attr_t *__restrict __attr,
                                 int *__restrict __policy)
noexcept
{
	always_assert(false);
	return EINVAL;
}

int
ircd_pthread_attr_setschedpolicy(pthread_attr_t *__attr,
                                 int __policy)
noexcept
{
	always_assert(false);
	return EINVAL;
}

int
ircd_pthread_attr_getinheritsched(const pthread_attr_t *__restrict __attr,
                                  int *__restrict __inherit)
noexcept
{
	always_assert(false);
	return EINVAL;
}

int
ircd_pthread_attr_setinheritsched(pthread_attr_t *__attr,
                                  int __inherit)
noexcept
{
	always_assert(false);
	return EINVAL;
}

int
ircd_pthread_attr_getscope(const pthread_attr_t *__restrict __attr,
                           int *__restrict __scope)
noexcept
{
	always_assert(false);
	return EINVAL;
}

int
ircd_pthread_attr_setscope(pthread_attr_t *__attr,
                           int __scope)
noexcept
{
	always_assert(false);
	return EINVAL;
}

int
ircd_pthread_attr_getstackaddr(const pthread_attr_t *__restrict __attr,
                               void **__restrict __stackaddr)
noexcept
{
	always_assert(false);
	return EINVAL;
}

int
ircd_pthread_attr_setstackaddr(pthread_attr_t *__attr,
                               void *__stackaddr)
noexcept
{
	always_assert(false);
	return EINVAL;
}

int
ircd_pthread_attr_getstacksize(const pthread_attr_t *__restrict __attr,
                               size_t *__restrict __stacksize)
noexcept
{
	always_assert(false);
	return EINVAL;
}

int
ircd_pthread_attr_setstacksize(pthread_attr_t *__attr,
                               size_t __stacksize)
noexcept
{
	always_assert(false);
	return EINVAL;
}

int
ircd_pthread_attr_getstack(const pthread_attr_t *__restrict __attr,
                           void **__restrict __stackaddr,
                           size_t *__restrict __stacksize)
noexcept
{
	always_assert(false);
	return EINVAL;
}

int
ircd_pthread_attr_setstack(pthread_attr_t *__attr,
                           void *__stackaddr,
                           size_t __stacksize)
noexcept
{
	always_assert(false);
	return EINVAL;
}

int
ircd_pthread_attr_setaffinity_np(pthread_attr_t *__attr,
                                 size_t __cpusetsize,
                                 const cpu_set_t *__cpuset)
noexcept
{
	always_assert(false);
	return EINVAL;
}

int
ircd_pthread_attr_getaffinity_np(const pthread_attr_t *__attr,
                                 size_t __cpusetsize,
                                 cpu_set_t *__cpuset)
noexcept
{
	always_assert(false);
	return EINVAL;
}

int
ircd_pthread_getattr_default_np(pthread_attr_t *__attr)
noexcept
{
	always_assert(false);
	return EINVAL;
}

int
ircd_pthread_setattr_default_np(const pthread_attr_t *__attr)
noexcept
{
	always_assert(false);
	return EINVAL;
}

int
ircd_pthread_getattr_np(pthread_t __th,
                        pthread_attr_t *__attr)
noexcept
{
	always_assert(false);
	return EINVAL;
}

///////////////////////////////////////////////////////////////////////////////
//
// Thread-Local
//

int
ircd_pthread_key_create(pthread_key_t *__key,
                        void (*__destr_function)(void *))
noexcept
{
	always_assert(false);
	return EINVAL;
}

int
ircd_pthread_key_delete(pthread_key_t __key)
noexcept
{
	always_assert(false);
	return EINVAL;
}

void *
ircd_pthread_getspecific(pthread_key_t __key)
noexcept
{
	always_assert(false);
	return nullptr;
}

int
ircd_pthread_setspecific(pthread_key_t __key,
                         const void *__pointer)
noexcept
{
	always_assert(false);
	return EINVAL;
}

///////////////////////////////////////////////////////////////////////////////
//
// Spinlock
//

int
ircd_pthread_spin_init(pthread_spinlock_t *__lock,
                       int __pshared)
noexcept
{
	always_assert(false);
	return EINVAL;
}

int
ircd_pthread_spin_destroy(pthread_spinlock_t *__lock)
noexcept
{
	always_assert(false);
	return EINVAL;
}

int
ircd_pthread_spin_lock(pthread_spinlock_t *__lock)
noexcept
{
	always_assert(false);
	return EINVAL;
}

int
ircd_pthread_spin_trylock(pthread_spinlock_t *__lock)
noexcept
{
	always_assert(false);
	return EINVAL;
}

int
ircd_pthread_spin_unlock(pthread_spinlock_t *__lock)
noexcept
{
	always_assert(false);
	return EINVAL;
}

///////////////////////////////////////////////////////////////////////////////
//
// Mutex
//

int
ircd_pthread_mutex_init(pthread_mutex_t *__mutex,
                        const pthread_mutexattr_t *__attr)
noexcept
{
	static_assert(sizeof(ircd::ctx::mutex) <= sizeof(pthread_mutex_t));
	assert(__mutex);
	//assert(__attr);

	auto *const mutex
	{
		reinterpret_cast<ircd::ctx::mutex *>(__mutex)
	};

	new (mutex) ircd::ctx::mutex;
	return 0;
}

int
ircd_pthread_mutex_destroy(pthread_mutex_t *__mutex)
noexcept
{
	assert(__mutex);
	auto *const mutex
	{
		reinterpret_cast<ircd::ctx::mutex *>(__mutex)
	};

	if(unlikely(mutex->locked()))
		return EBUSY;

	mutex->~mutex();
	return 0;
}

int
ircd_pthread_mutex_trylock(pthread_mutex_t *__mutex)
noexcept
{
	assert(__mutex);
	auto *const mutex
	{
		reinterpret_cast<ircd::ctx::mutex *>(__mutex)
	};

	if(!mutex->try_lock())
		return EBUSY;

	return 0;
}

int
ircd_pthread_mutex_lock(pthread_mutex_t *__mutex)
noexcept
{
	assert(__mutex);
	auto *const mutex
	{
		reinterpret_cast<ircd::ctx::mutex *>(__mutex)
	};

	#ifdef IRCD_PTHREAD_DEADLK_CHK
	if(unlikely(mutex->m == ircd::ctx::current))
		return EDEADLK;
	#endif

	mutex->lock();
	return 0;
}

int
ircd_pthread_mutex_timedlock(pthread_mutex_t *__restrict __mutex,
                             const struct timespec *__restrict __abstime)
noexcept
{
	always_assert(false);
	return EINVAL;
}

int
ircd_pthread_mutex_clocklock(pthread_mutex_t *__restrict __mutex,
                             clockid_t __clockid,
                             const struct timespec *__restrict __abstime)
noexcept
{
	always_assert(false);
	return EINVAL;
}

int
ircd_pthread_mutex_unlock(pthread_mutex_t *__mutex)
noexcept
{
	assert(__mutex);
	auto *const mutex
	{
		reinterpret_cast<ircd::ctx::mutex *>(__mutex)
	};

	if(unlikely(mutex->m != ircd::ctx::current))
		return EPERM;

	mutex->unlock();
	return 0;
}

int
ircd_pthread_mutex_getprioceiling(const pthread_mutex_t *__restrict __mutex,
                                  int *__restrict __prioceiling)
noexcept
{
	always_assert(false);
	return EINVAL;
}

int
ircd_pthread_mutex_setprioceiling(pthread_mutex_t *__restrict __mutex,
                                  int __prioceiling,
                                  int *__restrict __old_ceiling)
noexcept
{
	always_assert(false);
	return EINVAL;
}

int
ircd_pthread_mutex_consistent(pthread_mutex_t *__mutex)
noexcept
{
	always_assert(false);
	return EINVAL;
}

int
ircd_pthread_mutex_consistent_np(pthread_mutex_t *__mutex)
noexcept
{
	always_assert(false);
	return EINVAL;
}

//
// Mutex Attributes
//

int
ircd_pthread_mutexattr_init(pthread_mutexattr_t *__attr)
noexcept
{
	always_assert(false);
	return EINVAL;
}

int
ircd_pthread_mutexattr_destroy(pthread_mutexattr_t *__attr)
noexcept
{
	always_assert(false);
	return EINVAL;
}

int
ircd_pthread_mutexattr_getpshared(const pthread_mutexattr_t *__restrict __attr,
                                  int *__restrict __pshared)
noexcept
{
	always_assert(false);
	return EINVAL;
}

int
ircd_pthread_mutexattr_setpshared(pthread_mutexattr_t *__attr,
                                  int __pshared)
noexcept
{
	always_assert(false);
	return EINVAL;
}

int
ircd_pthread_mutexattr_gettype(const pthread_mutexattr_t *__restrict __attr,
                               int *__restrict __kind)
noexcept
{
	always_assert(false);
	return EINVAL;
}

int
ircd_pthread_mutexattr_settype(pthread_mutexattr_t *__attr,
                               int __kind)
noexcept
{
	always_assert(false);
	return EINVAL;
}

int
ircd_pthread_mutexattr_getprotocol(const pthread_mutexattr_t *__restrict __attr,
                                   int *__restrict __protocol)
noexcept
{
	always_assert(false);
	return EINVAL;
}

int
ircd_pthread_mutexattr_setprotocol(pthread_mutexattr_t *__attr,
                                   int __protocol)
noexcept
{
	always_assert(false);
	return EINVAL;
}

int
ircd_pthread_mutexattr_getprioceiling(const pthread_mutexattr_t *__restrict __attr,
                                      int *__restrict __prioceiling)
noexcept
{
	always_assert(false);
	return EINVAL;
}

int
ircd_pthread_mutexattr_setprioceiling(pthread_mutexattr_t *__attr,
                                      int __prioceiling)
noexcept
{
	always_assert(false);
	return EINVAL;
}

int
ircd_pthread_mutexattr_getrobust(const pthread_mutexattr_t *__attr,
                                 int *__robustness)
noexcept
{
	always_assert(false);
	return EINVAL;
}

int
ircd_pthread_mutexattr_getrobust_np(const pthread_mutexattr_t *__attr,
                                    int *__robustness)
noexcept
{
	always_assert(false);
	return EINVAL;
}

int
ircd_pthread_mutexattr_setrobust(pthread_mutexattr_t *__attr,
                                 int __robustness)
noexcept
{
	always_assert(false);
	return EINVAL;
}

int
ircd_pthread_mutexattr_setrobust_np(pthread_mutexattr_t *__attr,
                                    int __robustness)
noexcept
{
	always_assert(false);
	return EINVAL;
}

///////////////////////////////////////////////////////////////////////////////
//
// Shared Mutex
//

int
ircd_pthread_rwlock_init(pthread_rwlock_t *__restrict __rwlock,
                         const pthread_rwlockattr_t *__restrict __attr)
noexcept
{
	static_assert(sizeof(ircd::ctx::shared_mutex) <= sizeof(pthread_rwlock_t));
	assert(__rwlock);
	//assert(__attr);

	auto *const shared_mutex
	{
		reinterpret_cast<ircd::ctx::shared_mutex *>(__rwlock)
	};

	new (shared_mutex) ircd::ctx::shared_mutex;
	return 0;
}

int
ircd_pthread_rwlock_destroy(pthread_rwlock_t *__rwlock)
noexcept
{
	assert(__rwlock);
	auto *const shared_mutex
	{
		reinterpret_cast<ircd::ctx::shared_mutex *>(__rwlock)
	};

	const bool busy
	{
		!shared_mutex->can_lock_upgrade()
		|| shared_mutex->shares()
		|| shared_mutex->waiting()
	};

	if(unlikely(busy))
		return EBUSY;

	shared_mutex->~shared_mutex();
	return 0;
}

int
ircd_pthread_rwlock_rdlock(pthread_rwlock_t *__rwlock)
noexcept
{
	assert(__rwlock);
	auto *const shared_mutex
	{
		reinterpret_cast<ircd::ctx::shared_mutex *>(__rwlock)
	};

	shared_mutex->lock_shared();
	return 0;
}

int
ircd_pthread_rwlock_tryrdlock(pthread_rwlock_t *__rwlock)
noexcept
{
	assert(__rwlock);
	auto *const shared_mutex
	{
		reinterpret_cast<ircd::ctx::shared_mutex *>(__rwlock)
	};

	if(!shared_mutex->try_lock_shared())
		return EBUSY;

	return 0;
}

int
ircd_pthread_rwlock_timedrdlock(pthread_rwlock_t *__restrict __rwlock,
                                const struct timespec *__restrict __abstime)
noexcept
{
	always_assert(false);
	return EINVAL;
}

int
ircd_pthread_rwlock_clockrdlock(pthread_rwlock_t *__restrict __rwlock,
                                clockid_t __clockid,
                                const struct timespec *__restrict __abstime)
noexcept
{
	always_assert(false);
	return EINVAL;
}

int
ircd_pthread_rwlock_wrlock(pthread_rwlock_t *__rwlock)
noexcept
{
	assert(__rwlock);
	auto *const shared_mutex
	{
		reinterpret_cast<ircd::ctx::shared_mutex *>(__rwlock)
	};

	#ifdef IRCD_PTHREAD_DEADLK_CHK
	if(unlikely(shared_mutex->u == ircd::ctx::current))
		return EDEADLK;
	#endif

	shared_mutex->lock();
	return 0;
}

int
ircd_pthread_rwlock_trywrlock(pthread_rwlock_t *__rwlock)
noexcept
{
	assert(__rwlock);
	auto *const shared_mutex
	{
		reinterpret_cast<ircd::ctx::shared_mutex *>(__rwlock)
	};

	if(!shared_mutex->try_lock())
		return EBUSY;

	return 0;
}

int
ircd_pthread_rwlock_timedwrlock(pthread_rwlock_t *__restrict __rwlock,
                                const struct timespec *__restrict __abstime)
noexcept
{
	always_assert(false);
	return EINVAL;
}

int
ircd_pthread_rwlock_clockwrlock(pthread_rwlock_t *__restrict __rwlock,
                                clockid_t __clockid,
                                const struct timespec *__restrict __abstime)
noexcept
{
	always_assert(false);
	return EINVAL;
}

int
ircd_pthread_rwlock_unlock(pthread_rwlock_t *__rwlock)
noexcept
{
	assert(__rwlock);
	auto *const shared_mutex
	{
		reinterpret_cast<ircd::ctx::shared_mutex *>(__rwlock)
	};

	// pthread interface has no rdunlock() and wrunlock() so we have to branch
	if(shared_mutex->unique())
	{
		if(unlikely(shared_mutex->u != ircd::ctx::current))
			return EPERM;

		shared_mutex->unlock();
		return 0;
	}

	if(unlikely(shared_mutex->unique() || !shared_mutex->shares()))
		return EPERM;

	shared_mutex->unlock_shared();
	return 0;
}

//
// Shared Mutex Attributes
// 

int
ircd_pthread_rwlockattr_init(pthread_rwlockattr_t *__attr)
noexcept
{
	always_assert(false);
	return EINVAL;
}

int
ircd_pthread_rwlockattr_destroy(pthread_rwlockattr_t *__attr)
noexcept
{
	always_assert(false);
	return EINVAL;
}

int
ircd_pthread_rwlockattr_getpshared(const pthread_rwlockattr_t *__restrict __attr,
                                   int *__restrict __pshared)
noexcept
{
	always_assert(false);
	return EINVAL;
}

int
ircd_pthread_rwlockattr_setpshared(pthread_rwlockattr_t *__attr,
                                   int __pshared)
noexcept
{
	always_assert(false);
	return EINVAL;
}

int
ircd_pthread_rwlockattr_getkind_np(const pthread_rwlockattr_t *__restrict __attr,
                                   int *__restrict __pref)
noexcept
{
	always_assert(false);
	return EINVAL;
}

int
ircd_pthread_rwlockattr_setkind_np(pthread_rwlockattr_t *__attr,
                                   int __pref)
noexcept
{
	always_assert(false);
	return EINVAL;
}

///////////////////////////////////////////////////////////////////////////////
//
// Condition Variable
//

int
ircd_pthread_cond_init(pthread_cond_t *__restrict __cond,
                       const pthread_condattr_t *__restrict __cond_attr)
noexcept
{
	static_assert(sizeof(ircd::ctx::condition_variable) <= sizeof(pthread_cond_t));
	assert(__cond);
	//assert(__cond_attr);

	auto *const condition_variable
	{
		reinterpret_cast<ircd::ctx::condition_variable *>(__cond)
	};

	new (condition_variable) ircd::ctx::condition_variable;
	return 0;
}

int
ircd_pthread_cond_destroy(pthread_cond_t *__cond)
noexcept
{
	assert(__cond);
	auto *const condition_variable
	{
		reinterpret_cast<ircd::ctx::condition_variable *>(__cond)
	};

	const bool busy
	{
		!condition_variable->empty()
	};

	if(unlikely(busy))
		return EBUSY;

	condition_variable->~condition_variable();
	return 0;
}

int
ircd_pthread_cond_signal(pthread_cond_t *__cond)
noexcept
{
	auto *const condition_variable
	{
		reinterpret_cast<ircd::ctx::condition_variable *>(__cond)
	};

	condition_variable->notify();
	return 0;
}

int
ircd_pthread_cond_broadcast(pthread_cond_t *__cond)
noexcept
{
	auto *const condition_variable
	{
		reinterpret_cast<ircd::ctx::condition_variable *>(__cond)
	};

	condition_variable->notify_all();
	return 0;
}

int
ircd_pthread_cond_wait(pthread_cond_t *const __restrict __cond,
                       pthread_mutex_t *const __restrict __mutex)
noexcept
{
	assert(__cond);
	assert(__mutex);

	auto *const condition_variable
	{
		reinterpret_cast<ircd::ctx::condition_variable *>(__cond)
	};

	auto *const mutex
	{
		reinterpret_cast<ircd::ctx::mutex *>(__mutex)
	};

	condition_variable->wait(*mutex);
	return 0;
}

int
ircd_pthread_cond_timedwait(pthread_cond_t *const __restrict __cond,
                            pthread_mutex_t *const __restrict __mutex,
                            const struct timespec *__restrict __abstime)
noexcept
{
	using namespace std::chrono;

	assert(__cond);
	assert(__mutex);
	assert(__abstime);

	auto *const condition_variable
	{
		reinterpret_cast<ircd::ctx::condition_variable *>(__cond)
	};

	auto *const mutex
	{
		reinterpret_cast<ircd::ctx::mutex *>(__mutex)
	};

	const nanoseconds epoch
	{
		seconds(__abstime->tv_sec) +
		nanoseconds(__abstime->tv_nsec)
	};

	const time_point<system_clock, nanoseconds> time_point
	{
		epoch
	};

	const std::cv_status cv_status
	{
		condition_variable->wait_until(*mutex, time_point)
	};

	if(cv_status == std::cv_status::timeout)
		return ETIMEDOUT;

	return 0;
}

int
ircd_pthread_cond_clockwait(pthread_cond_t *__restrict __cond,
                            pthread_mutex_t *__restrict __mutex,
                            __clockid_t __clock_id,
                            const struct timespec *__restrict __abstime)
noexcept
{
	always_assert(false);
	return EINVAL;
}

//
// Condition Variable Attributes
//

int
ircd_pthread_condattr_init(pthread_condattr_t *__attr)
noexcept
{
	assert(__attr);
	memset(__attr, 0x0, sizeof(pthread_condattr_t));
	return 0;
}

int
ircd_pthread_condattr_destroy(pthread_condattr_t *__attr)
noexcept
{
	return 0;
}

int
ircd_pthread_condattr_getpshared(const pthread_condattr_t *__restrict __attr,
                                 int *__restrict __pshared)
noexcept
{
	always_assert(false);
	return EINVAL;
}

int
ircd_pthread_condattr_setpshared(pthread_condattr_t *__attr,
                                 int __pshared)
noexcept
{
	always_assert(false);
	return EINVAL;
}

int
ircd_pthread_condattr_getclock(const pthread_condattr_t *__restrict __attr,
                               __clockid_t *__restrict __clock_id)
noexcept
{
	always_assert(false);
	return EINVAL;
}

int
ircd_pthread_condattr_setclock(pthread_condattr_t *__attr,
                               __clockid_t __clock_id)
noexcept
{
	always_assert(false);
	return EINVAL;
}

///////////////////////////////////////////////////////////////////////////////
//
// Barrier
//

int
ircd_pthread_barrier_init(pthread_barrier_t *__restrict __barrier,
                          const pthread_barrierattr_t *__restrict __attr,
                          unsigned int __count)
noexcept
{
	always_assert(false);
	return EINVAL;
}

int
ircd_pthread_barrier_destroy(pthread_barrier_t *__barrier)
noexcept
{
	always_assert(false);
	return EINVAL;
}

int
ircd_pthread_barrier_wait(pthread_barrier_t *__barrier)
noexcept
{
	always_assert(false);
	return EINVAL;
}

//
// Barrier Attributes
//

int
ircd_pthread_barrierattr_init(pthread_barrierattr_t *__attr)
noexcept
{
	always_assert(false);
	return EINVAL;
}

int
ircd_pthread_barrierattr_destroy(pthread_barrierattr_t *__attr)
noexcept
{
	always_assert(false);
	return EINVAL;
}

int
ircd_pthread_barrierattr_getpshared(const pthread_barrierattr_t *__restrict __attr,
                                    int *__restrict __pshared)
noexcept
{
	always_assert(false);
	return EINVAL;
}

int
ircd_pthread_barrierattr_setpshared(pthread_barrierattr_t *__attr,
                                    int __pshared)
noexcept
{
	always_assert(false);
	return EINVAL;
}

//
// util
//

bool
ircd::ctx::posix::is(const pthread_t &target)
noexcept
{
	// Can't be an ircd::ctx if it's not the main thread, nor can we look.
	if(!is_main_thread())
		return false;

	const auto it
	{
		std::find_if(begin(ctxs), end(ctxs), [&]
		(const auto &context)
		{
			return id(context) == target;
		})
	};

	return it != end(ctxs);
}

bool
ircd::ctx::posix::hook_enabled()
noexcept
{
	// The hook is only enabled on the main thread.
	if(!is_main_thread())
		return false;

	// When disable_pthread is asserted, the hook is always enabled.
	if(ircd::ctx::posix::enable_hook > 0)
		return true;

	// When enable_pthread is asserted, the hook is never enabled.
	if(ircd::ctx::posix::enable_hook < 0)
		return false;

	// Consider the hook enabled if called from an ircd::ctx stack, since
	// that is clearly our code, and if we call into a library on such a
	// stack we will use an explicit enable_pthread if we need it.
	//
	// OTOH, when not on an ircd::ctx stack, we assume the call is coming from
	// some other code is running somewhere else in the address space, perhaps
	// totally unrelated, and we give that the expected unhooked behavior.
	return ircd::ctx::current != nullptr;
}

bool
ircd::ctx::posix::is_main_thread()
noexcept
{
	return ircd::ios::is_main_thread;
}
