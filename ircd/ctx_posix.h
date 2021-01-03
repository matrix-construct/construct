// The Construct
//
// Copyright (C) The Construct Developers, Authors & Contributors
// Copyright (C) 2016-2021 Jason Volk <jason@zemos.net>
//
// Permission to use, copy, modify, and/or distribute this software for any
// purpose with or without fee is hereby granted, provided that the above
// copyright notice and this permission notice is present in all copies. The
// full license for this software is available in the LICENSE file.

#pragma GCC visibility push(hidden)

#if !defined(__clockid_t)
typedef clockid_t __clockid_t;
#endif

extern "C" int
ircd_pthread_create(pthread_t *const thread,
                    const pthread_attr_t *const attr,
                    void *(*const start_routine)(void *),
                    void *const arg)
noexcept;

extern "C" int
ircd_pthread_join(pthread_t __th,
                  void **__thread_return)
noexcept;

extern "C" int
ircd_pthread_tryjoin_np(pthread_t __th,
                        void **__thread_return)
noexcept;

extern "C" int
ircd_pthread_timedjoin_np(pthread_t __th,
                          void **__thread_return,
                          const struct timespec *__abstime)
noexcept;

extern "C" int
ircd_pthread_clockjoin_np(pthread_t __th,
                          void **__thread_return,
                          clockid_t clockid,
                          const struct timespec *__abstime)
noexcept;

extern "C" int
ircd_pthread_detach(pthread_t __th)
noexcept;

extern "C" int
ircd_pthread_getcpuclockid(pthread_t __thread_id,
                           __clockid_t *__clock_id)
noexcept;

extern "C" int
ircd_pthread_atfork(void (*__prepare)(void),
                    void (*__parent)(void),
                    void (*__child)(void))
noexcept;

extern "C" void
ircd_pthread_exit(void *const retval)
noexcept;

extern "C" pthread_t
ircd_pthread_self(void)
noexcept;

//
// Initialization
//

extern "C" int
ircd_pthread_once(pthread_once_t *__once_control,
                  void (*__init_routine)(void))
noexcept;

//
// Cancellation
//

extern "C" int
ircd_pthread_setcancelstate(int __state,
                            int *__oldstate)
noexcept;

extern "C" int
ircd_pthread_setcanceltype(int __type,
                           int *__oldtype)
noexcept;

extern "C" int
ircd_pthread_cancel(pthread_t __th)
noexcept;

extern "C" void
ircd_pthread_testcancel(void)
noexcept;

//
// Scheduling
//

extern "C" int
ircd_pthread_setschedparam(pthread_t __target_thread,
                           int __policy,
                           const struct sched_param *__param)
noexcept;

extern "C" int
ircd_pthread_getschedparam(pthread_t __target_thread,
                           int *__restrict __policy,
                           struct sched_param *__restrict __param)
noexcept;

extern "C" int
ircd_pthread_setschedprio(pthread_t __target_thread,
                          int __prio)
noexcept;

extern "C" int
ircd_pthread_getname_np(pthread_t __target_thread,
                        char *__buf,
                        size_t __buflen)
noexcept;

extern "C" int
ircd_pthread_setname_np(pthread_t __target_thread,
                        const char *__name)
noexcept;

extern "C" int
ircd_pthread_getconcurrency(void)
noexcept;

extern "C" int
ircd_pthread_setconcurrency(int __level)
noexcept;

extern "C" int
ircd_pthread_setaffinity_np(pthread_t __th,
                            size_t __cpusetsize,
                            const cpu_set_t *__cpuset)
noexcept;

extern "C" int
ircd_pthread_getaffinity_np(pthread_t __th,
                            size_t __cpusetsize,
                            cpu_set_t *__cpuset)
noexcept;

extern "C" int
ircd_pthread_yield(void)
noexcept;

//
// Attributes
//

extern "C" int
ircd_pthread_attr_init(pthread_attr_t *__attr)
noexcept;

extern "C" int
ircd_pthread_attr_destroy(pthread_attr_t *__attr)
noexcept;

extern "C" int
ircd_pthread_attr_getdetachstate(const pthread_attr_t *__attr,
                                 int *__detachstate)
noexcept;

extern "C" int
ircd_pthread_attr_setdetachstate(pthread_attr_t *__attr,
                                 int __detachstate)
noexcept;

extern "C" int
ircd_pthread_attr_getguardsize(const pthread_attr_t *__attr,
                               size_t *__guardsize)
noexcept;

extern "C" int
ircd_pthread_attr_setguardsize(pthread_attr_t *__attr,
                               size_t __guardsize)
noexcept;

extern "C" int
ircd_pthread_attr_getschedparam(const pthread_attr_t *__restrict __attr,
                                struct sched_param *__restrict __param)
noexcept;

extern "C" int
ircd_pthread_attr_setschedparam(pthread_attr_t *__restrict __attr,
                                const struct sched_param *__restrict __param)
noexcept;

extern "C" int
ircd_pthread_attr_getschedpolicy(const pthread_attr_t *__restrict __attr,
                                 int *__restrict __policy)
noexcept;

extern "C" int
ircd_pthread_attr_setschedpolicy(pthread_attr_t *__attr,
                                 int __policy)
noexcept;

extern "C" int
ircd_pthread_attr_getinheritsched(const pthread_attr_t *__restrict __attr,
                                  int *__restrict __inherit)
noexcept;

extern "C" int
ircd_pthread_attr_setinheritsched(pthread_attr_t *__attr,
                                  int __inherit)
noexcept;

extern "C" int
ircd_pthread_attr_getscope(const pthread_attr_t *__restrict __attr,
                           int *__restrict __scope)
noexcept;

extern "C" int
ircd_pthread_attr_setscope(pthread_attr_t *__attr,
                           int __scope)
noexcept;

extern "C" int
ircd_pthread_attr_getstackaddr(const pthread_attr_t *__restrict __attr,
                               void **__restrict __stackaddr)
noexcept;

extern "C" int
ircd_pthread_attr_setstackaddr(pthread_attr_t *__attr,
                               void *__stackaddr)
noexcept;

extern "C" int
ircd_pthread_attr_getstacksize(const pthread_attr_t *__restrict __attr,
                               size_t *__restrict __stacksize)
noexcept;

extern "C" int
ircd_pthread_attr_setstacksize(pthread_attr_t *__attr,
                               size_t __stacksize)
noexcept;

extern "C" int
ircd_pthread_attr_getstack(const pthread_attr_t *__restrict __attr,
                           void **__restrict __stackaddr,
                           size_t *__restrict __stacksize)
noexcept;

extern "C" int
ircd_pthread_attr_setstack(pthread_attr_t *__attr,
                           void *__stackaddr,
                           size_t __stacksize)
noexcept;

extern "C" int
ircd_pthread_attr_setaffinity_np(pthread_attr_t *__attr,
                                 size_t __cpusetsize,
                                 const cpu_set_t *__cpuset)
noexcept;

extern "C" int
ircd_pthread_attr_getaffinity_np(const pthread_attr_t *__attr,
                                 size_t __cpusetsize,
                                 cpu_set_t *__cpuset)
noexcept;

extern "C" int
ircd_pthread_getattr_default_np(pthread_attr_t *__attr)
noexcept;

extern "C" int
ircd_pthread_setattr_default_np(const pthread_attr_t *__attr)
noexcept;

extern "C" int
ircd_pthread_getattr_np(pthread_t __th,
                        pthread_attr_t *__attr)
noexcept;

//
// Thread-Local
//

extern "C" int
ircd_pthread_key_create(pthread_key_t *__key,
                        void (*__destr_function)(void *))
noexcept;

extern "C" int
ircd_pthread_key_delete(pthread_key_t __key)
noexcept;

extern "C" void *
ircd_pthread_getspecific(pthread_key_t __key)
noexcept;

extern "C" int
ircd_pthread_setspecific(pthread_key_t __key,
                         const void *__pointer)
noexcept;

//
// Spinlock
//

extern "C" int
ircd_pthread_spin_init(pthread_spinlock_t *__lock,
                       int __pshared)
noexcept;

extern "C" int
ircd_pthread_spin_destroy(pthread_spinlock_t *__lock)
noexcept;

extern "C" int
ircd_pthread_spin_lock(pthread_spinlock_t *__lock)
noexcept;

extern "C" int
ircd_pthread_spin_trylock(pthread_spinlock_t *__lock)
noexcept;

extern "C" int
ircd_pthread_spin_unlock(pthread_spinlock_t *__lock)
noexcept;

//
// Mutex
//

extern "C" int
ircd_pthread_mutex_init(pthread_mutex_t *__mutex,
                        const pthread_mutexattr_t *__attr)
noexcept;

extern "C" int
ircd_pthread_mutex_destroy(pthread_mutex_t *__mutex)
noexcept;

extern "C" int
ircd_pthread_mutex_trylock(pthread_mutex_t *__mutex)
noexcept;

extern "C" int
ircd_pthread_mutex_lock(pthread_mutex_t *__mutex)
noexcept;

extern "C" int
ircd_pthread_mutex_timedlock(pthread_mutex_t *__restrict __mutex,
                             const struct timespec *__restrict __abstime)
noexcept;

extern "C" int
ircd_pthread_mutex_clocklock(pthread_mutex_t *__restrict __mutex,
                             clockid_t __clockid,
                             const struct timespec *__restrict __abstime)
noexcept;

extern "C" int
ircd_pthread_mutex_unlock(pthread_mutex_t *__mutex)
noexcept;

extern "C" int
ircd_pthread_mutex_getprioceiling(const pthread_mutex_t *__restrict __mutex,
                                  int *__restrict __prioceiling)
noexcept;

extern "C" int
ircd_pthread_mutex_setprioceiling(pthread_mutex_t *__restrict __mutex,
                                  int __prioceiling,
                                  int *__restrict __old_ceiling)
noexcept;

extern "C" int
ircd_pthread_mutex_consistent(pthread_mutex_t *__mutex)
noexcept;

extern "C" int
ircd_pthread_mutex_consistent_np(pthread_mutex_t *__mutex)
noexcept;

//
// Mutex Attributes
//

extern "C" int
ircd_pthread_mutexattr_init(pthread_mutexattr_t *__attr)
noexcept;

extern "C" int
ircd_pthread_mutexattr_destroy(pthread_mutexattr_t *__attr)
noexcept;

extern "C" int
ircd_pthread_mutexattr_getpshared(const pthread_mutexattr_t *__restrict __attr,
                                  int *__restrict __pshared)
noexcept;

extern "C" int
ircd_pthread_mutexattr_setpshared(pthread_mutexattr_t *__attr,
                                  int __pshared)
noexcept;

extern "C" int
ircd_pthread_mutexattr_gettype(const pthread_mutexattr_t *__restrict __attr,
                               int *__restrict __kind)
noexcept;

extern "C" int
ircd_pthread_mutexattr_settype(pthread_mutexattr_t *__attr,
                               int __kind)
noexcept;

extern "C" int
ircd_pthread_mutexattr_getprotocol(const pthread_mutexattr_t *__restrict __attr,
                                   int *__restrict __protocol)
noexcept;

extern "C" int
ircd_pthread_mutexattr_setprotocol(pthread_mutexattr_t *__attr,
                                   int __protocol)
noexcept;

extern "C" int
ircd_pthread_mutexattr_getprioceiling(const pthread_mutexattr_t *__restrict __attr,
                                      int *__restrict __prioceiling)
noexcept;

extern "C" int
ircd_pthread_mutexattr_setprioceiling(pthread_mutexattr_t *__attr,
                                      int __prioceiling)
noexcept;

extern "C" int
ircd_pthread_mutexattr_getrobust(const pthread_mutexattr_t *__attr,
                                 int *__robustness)
noexcept;

extern "C" int
ircd_pthread_mutexattr_getrobust_np(const pthread_mutexattr_t *__attr,
                                    int *__robustness)
noexcept;

extern "C" int
ircd_pthread_mutexattr_setrobust(pthread_mutexattr_t *__attr,
                                 int __robustness)
noexcept;

extern "C" int
ircd_pthread_mutexattr_setrobust_np(pthread_mutexattr_t *__attr,
                                    int __robustness)
noexcept;

//
// Shared Mutex
//

extern "C" int
ircd_pthread_rwlock_init(pthread_rwlock_t *__restrict __rwlock,
                         const pthread_rwlockattr_t *__restrict __attr)
noexcept;

extern "C" int
ircd_pthread_rwlock_destroy(pthread_rwlock_t *__rwlock)
noexcept;

extern "C" int
ircd_pthread_rwlock_rdlock(pthread_rwlock_t *__rwlock)
noexcept;

extern "C" int
ircd_pthread_rwlock_tryrdlock(pthread_rwlock_t *__rwlock)
noexcept;

extern "C" int
ircd_pthread_rwlock_timedrdlock(pthread_rwlock_t *__restrict __rwlock,
                                const struct timespec *__restrict __abstime)
noexcept;

extern "C" int
ircd_pthread_rwlock_clockrdlock(pthread_rwlock_t *__restrict __rwlock,
                                clockid_t __clockid,
                                const struct timespec *__restrict __abstime)
noexcept;

extern "C" int
ircd_pthread_rwlock_wrlock(pthread_rwlock_t *__rwlock)
noexcept;

extern "C" int
ircd_pthread_rwlock_trywrlock(pthread_rwlock_t *__rwlock)
noexcept;

extern "C" int
ircd_pthread_rwlock_timedwrlock(pthread_rwlock_t *__restrict __rwlock,
                                const struct timespec *__restrict __abstime)
noexcept;

extern "C" int
ircd_pthread_rwlock_clockwrlock(pthread_rwlock_t *__restrict __rwlock,
                                clockid_t __clockid,
                                const struct timespec *__restrict __abstime)
noexcept;

extern "C" int
ircd_pthread_rwlock_unlock(pthread_rwlock_t *__rwlock)
noexcept;

//
// Shared Mutex Attributes
// 

extern "C" int
ircd_pthread_rwlockattr_init(pthread_rwlockattr_t *__attr)
noexcept;

extern "C" int
ircd_pthread_rwlockattr_destroy(pthread_rwlockattr_t *__attr)
noexcept;

extern "C" int
ircd_pthread_rwlockattr_getpshared(const pthread_rwlockattr_t *__restrict __attr,
                                   int *__restrict __pshared)
noexcept;

extern "C" int
ircd_pthread_rwlockattr_setpshared(pthread_rwlockattr_t *__attr,
                                   int __pshared)
noexcept;

extern "C" int
ircd_pthread_rwlockattr_getkind_np(const pthread_rwlockattr_t *__restrict __attr,
                                   int *__restrict __pref)
noexcept;

extern "C" int
ircd_pthread_rwlockattr_setkind_np(pthread_rwlockattr_t *__attr,
                                   int __pref)
noexcept;

//
// Condition Variable
//

extern "C" int
ircd_pthread_cond_init(pthread_cond_t *__restrict __cond,
                       const pthread_condattr_t *__restrict __cond_attr)
noexcept;

extern "C" int
ircd_pthread_cond_destroy(pthread_cond_t *__cond)
noexcept;

extern "C" int
ircd_pthread_cond_signal(pthread_cond_t *__cond)
noexcept;

extern "C" int
ircd_pthread_cond_broadcast(pthread_cond_t *__cond)
noexcept;

extern "C" int
ircd_pthread_cond_wait(pthread_cond_t *const __restrict __cond,
                       pthread_mutex_t *const __restrict __mutex)
noexcept;

extern "C" int
ircd_pthread_cond_timedwait(pthread_cond_t *const __restrict __cond,
                            pthread_mutex_t *const __restrict __mutex,
                            const struct timespec *__restrict __abstime)
noexcept;

extern "C" int
ircd_pthread_cond_clockwait(pthread_cond_t *__restrict __cond,
                            pthread_mutex_t *__restrict __mutex,
                            __clockid_t __clock_id,
                            const struct timespec *__restrict __abstime)
noexcept;

//
// Condition Variable Attributes
//

extern "C" int
ircd_pthread_condattr_init(pthread_condattr_t *__attr)
noexcept;

extern "C" int
ircd_pthread_condattr_destroy(pthread_condattr_t *__attr)
noexcept;

extern "C" int
ircd_pthread_condattr_getpshared(const pthread_condattr_t *__restrict __attr,
                                 int *__restrict __pshared)
noexcept;

extern "C" int
ircd_pthread_condattr_setpshared(pthread_condattr_t *__attr,
                                 int __pshared)
noexcept;

extern "C" int
ircd_pthread_condattr_getclock(const pthread_condattr_t *__restrict __attr,
                               __clockid_t *__restrict __clock_id)
noexcept;

extern "C" int
ircd_pthread_condattr_setclock(pthread_condattr_t *__attr,
                               __clockid_t __clock_id)
noexcept;

//
// Barrier
//

extern "C" int
ircd_pthread_barrier_init(pthread_barrier_t *__restrict __barrier,
                          const pthread_barrierattr_t *__restrict __attr,
                          unsigned int __count)
noexcept;

extern "C" int
ircd_pthread_barrier_destroy(pthread_barrier_t *__barrier)
noexcept;

extern "C" int
ircd_pthread_barrier_wait(pthread_barrier_t *__barrier)
noexcept;

//
// Barrier Attributes
//

extern "C" int
ircd_pthread_barrierattr_init(pthread_barrierattr_t *__attr)
noexcept;

extern "C" int
ircd_pthread_barrierattr_destroy(pthread_barrierattr_t *__attr)
noexcept;

extern "C" int
ircd_pthread_barrierattr_getpshared(const pthread_barrierattr_t *__restrict __attr,
                                    int *__restrict __pshared)
noexcept;

extern "C" int
ircd_pthread_barrierattr_setpshared(pthread_barrierattr_t *__attr,
                                    int __pshared)
noexcept;

#pragma GCC visibility pop // hidden
