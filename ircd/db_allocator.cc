// The Construct
//
// Copyright (C) The Construct Developers, Authors & Contributors
// Copyright (C) 2016-2020 Jason Volk <jason@zemos.net>
//
// Permission to use, copy, modify, and/or distribute this software for any
// purpose with or without fee is hereby granted, provided that the above
// copyright notice and this permission notice is present in all copies. The
// full license for this software is available in the LICENSE file.

#include <RB_INC_SYS_MMAN_H
#include <RB_INC_JEMALLOC_H
#include "db.h"

#ifndef IRCD_DB_HAS_ALLOCATOR
	#warning "Consider upgrading to rocksdb 5.18+ for improved memory management."
#endif

#if defined(IRCD_ALLOCATOR_USE_JEMALLOC) && defined(HAVE_JEMALLOC_H)
	#define IRCD_DB_USE_JEMALLOC
#else
	#warning "Consider building with jemalloc for improved memory management."
#endif

//
// database::allocator
//

#ifdef IRCD_DB_HAS_ALLOCATOR

namespace ircd::db
{
	#ifdef IRCD_DB_USE_JEMALLOC
	static void *cache_arena_handle_alloc(extent_hooks_t *, void *, size_t, size_t, bool *, bool *, uint) noexcept;
	static bool cache_arena_handle_dalloc(extent_hooks_t *, void *, size_t, bool, uint) noexcept;
	static void cache_arena_handle_destroy(extent_hooks_t *, void *, size_t, bool, uint) noexcept;
	static bool cache_arena_handle_commit(extent_hooks_t *, void *, size_t, size_t, size_t, uint) noexcept;
	static bool cache_arena_handle_decommit(extent_hooks_t *, void *, size_t, size_t, size_t, uint) noexcept;
	static bool cache_arena_handle_purge_lazy(extent_hooks_t *, void *, size_t, size_t, size_t, uint) noexcept;
	static bool cache_arena_handle_purge_forced(extent_hooks_t *, void *, size_t, size_t, size_t, uint) noexcept;
	static bool cache_arena_handle_split(extent_hooks_t *, void *, size_t, size_t, size_t, bool, uint) noexcept;
	static bool cache_arena_handle_merge(extent_hooks_t *, void *, size_t, void *, size_t, bool, uint) noexcept;
	thread_local extent_hooks_t *their_cache_arena_hooks, cache_arena_hooks;
	#endif
}

decltype(ircd::db::database::allocator::ALIGN_DEFAULT)
ircd::db::database::allocator::ALIGN_DEFAULT
{
	#if defined(__AVX512F__)
		64
	#elif defined(__AVX__)
		32
	#elif defined(__SSE__)
		16
	#else
		sizeof(void *)
	#endif
};

decltype(ircd::db::database::allocator::mlock_limit)
ircd::db::database::allocator::mlock_limit
{
	ircd::allocator::rlimit_memlock()
};

decltype(ircd::db::database::allocator::mlock_enabled)
ircd::db::database::allocator::mlock_enabled
{
	mlock_limit == -1UL

	// mlock2() not supported by valgrind
	&& !vg::active
};

decltype(ircd::db::database::allocator::mlock_current)
ircd::db::database::allocator::mlock_current;

/// Handle to a jemalloc arena when non-zero. Used as the base arena for all
/// cache allocators.
decltype(ircd::db::database::allocator::cache_arena)
ircd::db::database::allocator::cache_arena;

void
ircd::db::database::allocator::init()
{
	#ifdef IRCD_DB_USE_JEMALLOC
	cache_arena = ircd::allocator::get<unsigned>("arenas.create");

	char extent_hooks_keybuf[32];
	const string_view cache_arena_hooks_key{fmt::sprintf
	{
		extent_hooks_keybuf, "arena.%u.extent_hooks", cache_arena
	}};

	cache_arena_hooks.alloc = cache_arena_handle_alloc;
	cache_arena_hooks.dalloc = cache_arena_handle_dalloc;
	cache_arena_hooks.destroy = cache_arena_handle_destroy;
	cache_arena_hooks.commit = cache_arena_handle_commit;
	cache_arena_hooks.decommit = cache_arena_handle_decommit;
	cache_arena_hooks.purge_lazy = cache_arena_handle_purge_lazy;
	cache_arena_hooks.purge_forced = cache_arena_handle_purge_forced;
	cache_arena_hooks.split = cache_arena_handle_split;
	cache_arena_hooks.merge = cache_arena_handle_merge;
	ircd::allocator::set(cache_arena_hooks_key, &cache_arena_hooks, their_cache_arena_hooks);
	assert(their_cache_arena_hooks);
	#endif
}

void
ircd::db::database::allocator::fini()
noexcept
{
	#ifdef IRCD_DB_USE_JEMALLOC
	if(likely(cache_arena != 0))
	{
		char keybuf[64];
		ircd::allocator::get<void>(string_view(fmt::sprintf
		{
			keybuf, "arena.%u.reset", cache_arena
		}));

		ircd::allocator::get<void>(string_view(fmt::sprintf
		{
			keybuf, "arena.%u.destroy", cache_arena
		}));
	}
	#endif
}

#ifdef IRCD_DB_USE_JEMALLOC
void *
ircd::db::cache_arena_handle_alloc(extent_hooks_t *const hooks,
                                   void *const new_addr,
                                   size_t size,
                                   size_t alignment,
                                   bool *const zero,
                                   bool *const commit,
                                   unsigned arena_ind)
noexcept
{
	assert(their_cache_arena_hooks);
	const auto &their_hooks(*their_cache_arena_hooks);

	#ifdef RB_DEBUG_DB_ENV
	assert(zero);
	assert(commit);
	log::debug
	{
		log, "cache arena:%u alloc addr:%p size:%zu align:%zu z:%b c:%b ind:%u",
		database::allocator::cache_arena,
		new_addr,
		size,
		alignment,
		*zero,
		*commit,
		arena_ind,
	};
	#endif

	void *const ret
	{
		their_hooks.alloc(hooks, new_addr, size, alignment, zero, commit, arena_ind)
	};

	// This feature is only enabled when RLIMIT_MEMLOCK is unlimited. We don't
	// want to deal with any limit at all.
	#if defined(HAVE_MLOCK2) && defined(MLOCK_ONFAULT)
	if(database::allocator::mlock_enabled)
	{
		syscall(::mlock2, ret, size, MLOCK_ONFAULT);
		database::allocator::mlock_current += size;
	}
	#endif

	return ret;
}
#endif

#ifdef IRCD_DB_USE_JEMALLOC
bool
ircd::db::cache_arena_handle_dalloc(extent_hooks_t *hooks,
                                    void *const ptr,
                                    size_t size,
                                    bool committed,
                                    uint arena_ind)
noexcept
{
	assert(their_cache_arena_hooks);
	const auto &their_hooks(*their_cache_arena_hooks);

	#ifdef RB_DEBUG_DB_ENV
	log::debug
	{
		log, "cache arena:%u dalloc addr:%p size:%zu align:%zu z:%b c:%b ind:%u",
		database::allocator::cache_arena,
		ptr,
		size,
		committed,
		arena_ind,
	};
	#endif

	const bool ret
	{
		their_hooks.dalloc(hooks, ptr, size, committed, arena_ind)
	};

	#if defined(HAVE_MLOCK2)
	if(database::allocator::mlock_current && !ret)
	{
		syscall(::munlock, ptr, size);
		assert(database::allocator::mlock_current >= size);
		database::allocator::mlock_current -= size;
	}
	#endif

	return ret;
}
#endif

#ifdef IRCD_DB_USE_JEMALLOC
void
ircd::db::cache_arena_handle_destroy(extent_hooks_t *hooks,
                                     void *const ptr,
                                     size_t size,
                                     bool committed,
                                     uint arena_ind)
noexcept
{
	assert(their_cache_arena_hooks);
	const auto &their_hooks(*their_cache_arena_hooks);

	#ifdef RB_DEBUG_DB_ENV
	log::debug
	{
		log, "cache arena:%u destroy addr:%p size:%zu align:%zu z:%b c:%b ind:%u",
		database::allocator::cache_arena,
		ptr,
		size,
		committed,
		arena_ind,
	};
	#endif

	#if defined(HAVE_MLOCK2)
	if(database::allocator::mlock_current)
	{
		syscall(::munlock, ptr, size);
		assert(database::allocator::mlock_current >= size);
		database::allocator::mlock_current -= size;
	}
	#endif

	return their_hooks.destroy(hooks, ptr, size, committed, arena_ind);
}
#endif

#ifdef IRCD_DB_USE_JEMALLOC
bool
ircd::db::cache_arena_handle_commit(extent_hooks_t *const hooks,
                                    void *const ptr,
                                    size_t size,
                                    size_t offset,
                                    size_t length,
                                    uint arena_ind)
noexcept
{
	assert(their_cache_arena_hooks);
	const auto &their_hooks(*their_cache_arena_hooks);

	#ifdef RB_DEBUG_DB_ENV
	log::debug
	{
		log, "cache arena:%u commit addr:%p size:%zu offset:%zu length:%zu ind:%u",
		database::allocator::cache_arena,
		ptr,
		size,
		offset,
		length,
		arena_ind,
	};
	#endif

	return their_hooks.commit(hooks, ptr, size, offset, length, arena_ind);
}
#endif

#ifdef IRCD_DB_USE_JEMALLOC
bool
ircd::db::cache_arena_handle_decommit(extent_hooks_t *const hooks,
                                      void *const ptr,
                                      size_t size,
                                      size_t offset,
                                      size_t length,
                                      uint arena_ind)
noexcept
{
	assert(their_cache_arena_hooks);
	const auto &their_hooks(*their_cache_arena_hooks);

	#ifdef RB_DEBUG_DB_ENV
	log::debug
	{
		log, "cache arena:%u decommit addr:%p size:%zu offset:%zu length:%zu ind:%u",
		database::allocator::cache_arena,
		ptr,
		size,
		offset,
		length,
		arena_ind,
	};
	#endif

	return their_hooks.decommit(hooks, ptr, size, offset, length, arena_ind);
}
#endif

#ifdef IRCD_DB_USE_JEMALLOC
bool
ircd::db::cache_arena_handle_purge_lazy(extent_hooks_t *const hooks,
                                        void *const ptr,
                                        size_t size,
                                        size_t offset,
                                        size_t length,
                                        uint arena_ind)
noexcept
{
	assert(their_cache_arena_hooks);
	const auto &their_hooks(*their_cache_arena_hooks);

	#ifdef RB_DEBUG_DB_ENV
	log::debug
	{
		log, "cache arena:%u purge lazy addr:%p size:%zu offset:%zu length:%zu ind:%u",
		database::allocator::cache_arena,
		ptr,
		size,
		offset,
		length,
		arena_ind,
	};
	#endif

	return their_hooks.purge_lazy(hooks, ptr, size, offset, length, arena_ind);
}
#endif

#ifdef IRCD_DB_USE_JEMALLOC
bool
ircd::db::cache_arena_handle_purge_forced(extent_hooks_t *const hooks,
                                          void *const ptr,
                                          size_t size,
                                          size_t offset,
                                          size_t length,
                                          uint arena_ind)
noexcept
{
	assert(their_cache_arena_hooks);
	const auto &their_hooks(*their_cache_arena_hooks);

	#ifdef RB_DEBUG_DB_ENV
	log::debug
	{
		log, "cache arena:%u purge forced addr:%p size:%zu offset:%zu length:%zu ind:%u",
		database::allocator::cache_arena,
		ptr,
		size,
		offset,
		length,
		arena_ind,
	};
	#endif

	return their_hooks.purge_forced(hooks, ptr, size, offset, length, arena_ind);
}
#endif

#ifdef IRCD_DB_USE_JEMALLOC
bool
ircd::db::cache_arena_handle_split(extent_hooks_t *const hooks,
                                   void *const ptr,
                                   size_t size,
                                   size_t size_a,
                                   size_t size_b,
                                   bool committed,
                                   uint arena_ind)
noexcept
{
	assert(their_cache_arena_hooks);
	const auto &their_hooks(*their_cache_arena_hooks);

	#ifdef RB_DEBUG_DB_ENV
	log::debug
	{
		log, "cache arena:%u split addr:%p size:%zu size_a:%zu size_b:%zu committed:%b ind:%u",
		database::allocator::cache_arena,
		ptr,
		size,
		size_a,
		size_b,
		committed,
		arena_ind,
	};
	#endif

	return their_hooks.split(hooks, ptr, size, size_a, size_b, committed, arena_ind);
}
#endif

#ifdef IRCD_DB_USE_JEMALLOC
bool
ircd::db::cache_arena_handle_merge(extent_hooks_t *const hooks,
                                   void *const addr_a,
                                   size_t size_a,
                                   void *const addr_b,
                                   size_t size_b,
                                   bool committed,
                                   uint arena_ind)
noexcept
{
	assert(their_cache_arena_hooks);
	const auto &their_hooks(*their_cache_arena_hooks);

	#ifdef RB_DEBUG_DB_ENV
	log::debug
	{
		log, "cache arena:%u merge a[addr:%p size:%zu] b[addr:%p size:%zu] committed:%b ind:%u",
		database::allocator::cache_arena,
		addr_a,
		size_a,
		addr_b,
		size_b,
		committed,
		arena_ind,
	};
	#endif

	return their_hooks.merge(hooks, addr_a, size_a, addr_b, size_b, committed, arena_ind);
}
#endif

//
// allocator::allocator
//

ircd::db::database::allocator::allocator(database *const &d,
                                         database::column *const &c,
                                         const unsigned &arena,
                                         const size_t &alignment)
:d{d}
,c{c}
,alignment{alignment}
,arena{arena}
,arena_flags
{
	0
	#ifdef IRCD_DB_USE_JEMALLOC
	| MALLOCX_ARENA(this->arena)
	| MALLOCX_ALIGN(this->alignment)
	| MALLOCX_TCACHE_NONE
	#endif
}
{
	assert(math::is_pow2(alignment));
}

ircd::db::database::allocator::~allocator()
noexcept
{
}

size_t
ircd::db::database::allocator::UsableSize(void *const ptr,
                                          size_t size)
const noexcept
{
	const size_t ret
	{
		#ifdef IRCD_DB_USE_JEMALLOC
			sallocx(ptr, arena_flags)
		#else
			size % alignment != 0?
				size + (alignment - (size % alignment)):
				size
		#endif
	};

	#ifndef IRCD_DB_USE_JEMALLOC
		assert(ret % alignment == 0);
		assert(alignment % sizeof(void *) == 0);
	#endif

	return ret;
}

void
ircd::db::database::allocator::Deallocate(void *const ptr)
noexcept
{
	#ifdef IRCD_DB_USE_JEMALLOC
		dallocx(ptr, arena_flags);
	#else
		std::free(ptr);
	#endif
}

void *
ircd::db::database::allocator::Allocate(size_t size)
noexcept
{
	assert(size > 0UL);
	assert(size < 256_GiB);

	const auto ptr
	{
		#ifdef IRCD_DB_USE_JEMALLOC
			mallocx(size, arena_flags)
		#else
			ircd::allocator::aligned_alloc(alignment, size).release()
		#endif
	};

	#ifdef RB_DEBUG_DB_ENV
	assert(d);
	log::debug
	{
		log, "[%s]'%s' allocate:%zu alignment:%zu %p",
		db::name(*d),
		c? string_view(db::name(*c)): string_view{},
		size,
		alignment,
		ptr,
	};
	#endif

	return ptr;
}

const char *
ircd::db::database::allocator::Name()
const noexcept
{
	return c? db::name(*c).c_str():
	       d? db::name(*d).c_str():
	       "unaffiliated";
}

#endif IRCD_DB_HAS_ALLOCATOR
