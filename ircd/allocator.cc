// Matrix Construct
//
// Copyright (C) Matrix Construct Developers, Authors & Contributors
// Copyright (C) 2016-2018 Jason Volk <jason@zemos.net>
//
// Permission to use, copy, modify, and/or distribute this software for any
// purpose with or without fee is hereby granted, provided that the above
// copyright notice and this permission notice is present in all copies. The
// full license for this software is available in the LICENSE file.

#include <RB_INC_MALLOC_H

// Uncomment or -D this #define to enable our own crude but simple ability to
// profile dynamic memory usage. Global `new` and `delete` will be captured
// here by this definition file into thread_local counters accessible via
// ircd::allocator::profile. This feature allows the developer to find out if
// allocations are occurring during some scope by sampling the counters.
//
// #define RB_PROF_ALLOC

#if defined(__GNU_LIBRARY__) && defined(HAVE_MALLOC_H)
ircd::string_view
ircd::allocator::info(const mutable_buffer &buf)
{
	std::stringstream out;
	pubsetbuf(out, buf);

	const auto ma
	{
		::mallinfo()
	};

	out << "arena:     " << pretty(iec(ma.arena)) << std::endl
	    << "ordblks:   " << ma.ordblks << std::endl
	    << "smblks:    " << ma.smblks << std::endl
	    << "hblks:     " << ma.hblks << std::endl
	    << "hblkhd:    " << pretty(iec(ma.hblkhd)) << std::endl
	    << "usmblks:   " << pretty(iec(ma.usmblks)) << std::endl
	    << "fsmblks:   " << pretty(iec(ma.fsmblks)) << std::endl
	    << "uordblks:  " << pretty(iec(ma.uordblks)) << std::endl
	    << "fordblks:  " << pretty(iec(ma.fordblks)) << std::endl
	    << "keepcost:  " << pretty(iec(ma.keepcost)) << std::endl
	    ;

	return view(out, buf);
}
#else
ircd::string_view
ircd::allocator::info(const mutable_buffer &buf)
{
	return {};
}
#endif

#if defined(__GNU_LIBRARY__) && defined(HAVE_MALLOC_H)
bool
ircd::allocator::trim(const size_t &pad)
{
	return malloc_trim(pad);
}
#else
bool
ircd::allocator::trim(const size_t &pad)
{
	return false;
}
#endif

//
// allocator::state
//

void
ircd::allocator::state::deallocate(const uint &pos,
                                   const size_type &n)
{
	for(size_t i(0); i < n; ++i)
	{
		assert(test(pos + i));
		btc(pos + i);
	}

	last = pos;
}

uint
ircd::allocator::state::allocate(const size_type &n,
                                 const uint &hint)
{
	const auto ret
	{
		allocate(std::nothrow, n, hint)
	};

	if(unlikely(ret >= size))
		throw std::bad_alloc();

	return ret;
}

uint
ircd::allocator::state::allocate(std::nothrow_t,
                                 const size_type &n,
                                 const uint &hint)
{
	const auto next(this->next(n));
	if(unlikely(next >= size))         // No block of n was found anywhere (next is past-the-end)
		return next;

	for(size_t i(0); i < n; ++i)
	{
		assert(!test(next + i));
		bts(next + i);
	}

	last = next + n;
	return next;
}

uint
ircd::allocator::state::next(const size_t &n)
const
{
	uint ret(last), rem(n);
	for(; ret < size && rem; ++ret)
		if(test(ret))
			rem = n;
		else
			--rem;

	if(likely(!rem))
		return ret - n;

	for(ret = 0, rem = n; ret < last && rem; ++ret)
		if(test(ret))
			rem = n;
		else
			--rem;

	if(unlikely(rem))                  // The allocator should throw std::bad_alloc if !rem
		return size;

	return ret - n;
}

bool
ircd::allocator::state::available(const size_t &n)
const
{
	return this->next(n) < size;
}

//
// allocator::scope
//

namespace ircd::allocator
{
	void *(*their_malloc_hook)(size_t, const void *);
	static void *malloc_hook(size_t, const void *);
	static void install_malloc_hook();
	static void uninstall_malloc_hook();

	void *(*their_realloc_hook)(void *, size_t, const void *);
	static void *realloc_hook(void *, size_t, const void *);
	static void install_realloc_hook();
	static void uninstall_realloc_hook();

	void (*their_free_hook)(void *, const void *);
	static void free_hook(void *, const void *);
	static void install_free_hook();
	static void uninstall_free_hook();
}

decltype(ircd::allocator::scope::current)
ircd::allocator::scope::current;

ircd::allocator::scope::scope(alloc_closure ac,
                              realloc_closure rc,
                              free_closure fc)
:theirs
{
	current
}
,user_alloc
{
	std::move(ac)
}
,user_realloc
{
	std::move(rc)
}
,user_free
{
	std::move(fc)
}
{
	// If an allocator::scope instance already exists somewhere
	// up the stack, *current will already be set. We only install
	// our global hook handlers at the first instance ctor and
	// uninstall it after that first instance dtors.
	if(!current)
	{
		install_malloc_hook();
		install_realloc_hook();
		install_free_hook();
	}

	current = this;
}

ircd::allocator::scope::~scope()
noexcept
{
	assert(current);
	current = theirs;

	// Reinstall the pre-existing hooks after our last scope instance
	// has destructed (the first to have constructed). We know this when
	// current becomes null.
	if(!current)
	{
		uninstall_malloc_hook();
		uninstall_realloc_hook();
		uninstall_free_hook();
	}
}

void
ircd::allocator::free_hook(void *const ptr,
                           const void *const caller)
{
	// Once we've hooked we put back their hook before calling the user
	// so they can passthru to the function without hooking themselves.
	uninstall_free_hook();
	const unwind rehook_ours
	{
		install_free_hook
	};

	assert(scope::current);
	if(scope::current->user_free)
		scope::current->user_free(ptr);
	else
		::free(ptr);
}

void *
ircd::allocator::realloc_hook(void *const ptr,
                              size_t size,
                              const void *const caller)
{
	// Once we've hooked we put back their hook before calling the user
	// so they can passthru to the function without hooking themselves.
	uninstall_realloc_hook();
	const unwind rehook_ours
	{
		install_realloc_hook
	};

	assert(scope::current);
	return scope::current->user_realloc?
		scope::current->user_realloc(ptr, size):
		::realloc(ptr, size);
}

void *
ircd::allocator::malloc_hook(size_t size,
                             const void *const caller)
{
	// Once we've hooked we put back their hook before calling the user
	// so they can passthru to the function without hooking themselves.
	uninstall_malloc_hook();
	const unwind rehook_ours
	{
		install_malloc_hook
	};

	assert(scope::current);
	return scope::current->user_alloc?
		scope::current->user_alloc(size):
		::malloc(size);
}

#if defined(__GNU_LIBRARY__) && defined(HAVE_MALLOC_H)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"
void
ircd::allocator::install_malloc_hook()
{
	assert(!their_malloc_hook);
	their_malloc_hook = __malloc_hook;
	__malloc_hook = malloc_hook;
}
#pragma GCC diagnostic pop
#else
void
ircd::allocator::install_malloc_hook()
{
}
#endif

#if defined(__GNU_LIBRARY__) && defined(HAVE_MALLOC_H)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"
void
ircd::allocator::uninstall_malloc_hook()
{
	__malloc_hook = their_malloc_hook;
}
#pragma GCC diagnostic pop
#else
void
ircd::allocator::uninstall_malloc_hook()
{
}
#endif

#if defined(__GNU_LIBRARY__) && defined(HAVE_MALLOC_H)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"
void
ircd::allocator::install_realloc_hook()
{
	assert(!their_realloc_hook);
	their_realloc_hook = __realloc_hook;
	__realloc_hook = realloc_hook;
}
#pragma GCC diagnostic pop
#else
void
ircd::allocator::install_realloc_hook()
{
}
#endif

#if defined(__GNU_LIBRARY__) && defined(HAVE_MALLOC_H)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"
void
ircd::allocator::uninstall_realloc_hook()
{
	__realloc_hook = their_realloc_hook;
}
#else
void
ircd::allocator::uninstall_realloc_hook()
{
}
#endif

#if defined(__GNU_LIBRARY__) && defined(HAVE_MALLOC_H)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"
void
ircd::allocator::install_free_hook()
{
	assert(!their_free_hook);
	their_free_hook = __free_hook;
	__free_hook = free_hook;
}
#pragma GCC diagnostic pop
#else
void
ircd::allocator::install_free_hook()
{
}
#endif

#if defined(__GNU_LIBRARY__) && defined(HAVE_MALLOC_H)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"
void
ircd::allocator::uninstall_free_hook()
{
	__free_hook = their_free_hook;
}
#pragma GCC diagnostic pop
#else
void
ircd::allocator::uninstall_free_hook()
{
}
#endif

//
// allocator::profile
//

thread_local ircd::allocator::profile
ircd::allocator::profile::this_thread
{};

ircd::allocator::profile
ircd::allocator::operator-(const profile &a,
                           const profile &b)
{
	profile ret(a);
	ret -= b;
	return ret;
}

ircd::allocator::profile
ircd::allocator::operator+(const profile &a,
                           const profile &b)
{
	profile ret(a);
	ret += b;
	return ret;
}

ircd::allocator::profile &
ircd::allocator::operator-=(profile &a,
                            const profile &b)
{
	a.alloc_count -= b.alloc_count;
	a.free_count -= b.free_count;
	a.alloc_bytes -= b.alloc_bytes;
	a.free_bytes -= b.free_bytes;
	return a;
}

ircd::allocator::profile &
ircd::allocator::operator+=(profile &a,
                            const profile &b)
{
	a.alloc_count += b.alloc_count;
	a.free_count += b.free_count;
	a.alloc_bytes += b.alloc_bytes;
	a.free_bytes += b.free_bytes;
	return a;
}

#ifdef RB_PROF_ALLOC // --------------------------------------------------

__attribute__((alloc_size(1), malloc, returns_nonnull))
void *
operator new(const size_t size)
{
	void *const &ptr(::malloc(size));
	if(unlikely(!ptr))
		throw std::bad_alloc();

	auto &this_thread(ircd::allocator::profile::this_thread);
	this_thread.alloc_bytes += size;
	this_thread.alloc_count++;

	return ptr;
}

void
operator delete(void *const ptr)
{
	::free(ptr);

	auto &this_thread(ircd::allocator::profile::this_thread);
	this_thread.free_count++;
}

void
operator delete(void *const ptr,
                const size_t size)
{
	::free(ptr);

	auto &this_thread(ircd::allocator::profile::this_thread);
	this_thread.free_bytes += size;
	this_thread.free_count++;
}

#endif // RB_PROF_ALLOC --------------------------------------------------
