// Matrix Construct
//
// Copyright (C) Matrix Construct Developers, Authors & Contributors
// Copyright (C) 2016-2018 Jason Volk <jason@zemos.net>
//
// Permission to use, copy, modify, and/or distribute this software for any
// purpose with or without fee is hereby granted, provided that the above
// copyright notice and this permission notice is present in all copies. The
// full license for this software is available in the LICENSE file.

// Uncomment or -D this #define to enable our own crude but simple ability to
// profile dynamic memory usage. Global `new` and `delete` will be captured
// here by this definition file into thread_local counters accessible via
// ircd::allocator::profile. This feature allows the developer to find out if
// allocations are occurring during some scope by sampling the counters.
//
// #define RB_PROF_ALLOC

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
	const auto next(this->next(n));
	if(unlikely(next >= size))         // No block of n was found anywhere (next is past-the-end)
		throw std::bad_alloc();

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
