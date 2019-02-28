// Matrix Construct
//
// Copyright (C) Matrix Construct Developers, Authors & Contributors
// Copyright (C) 2016-2018 Jason Volk <jason@zemos.net>
//
// Permission to use, copy, modify, and/or distribute this software for any
// purpose with or without fee is hereby granted, provided that the above
// copyright notice and this permission notice is present in all copies. The
// full license for this software is available in the LICENSE file.

#pragma once
#define HAVE_IRCD_ALLOCATOR_H

/// Suite of custom allocator templates for special behavior and optimization
///
/// These tools can be used as alternatives to the standard allocator. Most
/// templates implement the std::allocator concept and can be used with
/// std:: containers by specifying them in the container's template parameter.
///
namespace ircd::allocator
{
	struct state;
	struct scope;
	struct profile;
	template<class T = char> struct dynamic;
	template<class T = char, size_t = 512> struct fixed;
	template<class T = char, size_t L0_SIZE = 512> struct twolevel;
	template<class T> struct node;

	profile &operator+=(profile &, const profile &);
	profile &operator-=(profile &, const profile &);
	profile operator+(const profile &, const profile &);
	profile operator-(const profile &, const profile &);

	bool trim(const size_t &pad = 0); // malloc_trim(3)
	string_view info(const mutable_buffer &);
};

/// Profiling counters. The purpose of this device is to gauge whether unwanted
/// or non-obvious allocations are taking place for a specific section. This
/// profiler has that very specific purpose and is not a replacement for
/// full-fledged memory profiling. This works by replacing global operator new
/// and delete, not any deeper hooks on malloc() at this time. To operate this
/// device you must first recompile and relink with RB_PROF_ALLOC defined at
/// least for the ircd/allocator.cc unit.
///
/// 1. Create an instance by copying the this_thread variable which will
/// snapshot the current counters.
/// 2. At some later point, create a second instance by copying the
/// this_thread variable again.
/// 3. Use the arithmetic operators to compute the difference between the two
/// snapshots and the result will be the count isolated between them.
struct ircd::allocator::profile
{
	uint64_t alloc_count {0};
	uint64_t free_count {0};
	size_t alloc_bytes {0};
	size_t free_bytes {0};

	/// Explicitly enabled by define at compile time only. Note: replaces
	/// global `new` and `delete` when enabled.
	static thread_local profile this_thread;
};

/// This object hooks and replaces global ::malloc() and family for the
/// lifetime of the instance, redirecting those calls to the user's provided
/// callbacks. This functionality may not be available on all platforms so it
/// cannot be soley relied upon in a production release. It may still be used
/// optimistically as an optimization in production.
///
/// This device is useful to control dynamic memory at level where specific
/// class allocators are too fine-grained and replacing global new is too
/// coarse (and far too intrusive to the whole process). Instead this works
/// on the stack for everything further up the stack.
///
/// This class is friendly. It takes control from any other previous instance
/// of allocator::scope and then restores their control after this goes out of
/// scope. Once all instances of allocator::scope go out of scope, the previous
/// global __malloc_hook is reinstalled.
///
struct ircd::allocator::scope
{
	using alloc_closure = std::function<void *(const size_t &)>;
	using realloc_closure = std::function<void *(void *const &ptr, const size_t &)>;
	using free_closure = std::function<void (void *const &ptr)>;

	static scope *current;
	scope *theirs;
	alloc_closure user_alloc;
	realloc_closure user_realloc;
	free_closure user_free;

  public:
	scope(alloc_closure = {}, realloc_closure = {}, free_closure = {});
	scope(const scope &) = delete;
	scope(scope &&) = delete;
	~scope();
};

/// Internal state structure for some of these tools. This is a very small and
/// simple interface to a bit array representing the availability of an element
/// in a pool of elements. The actual array of the proper number of bits must
/// be supplied by the user of the state. The allocator using this interface
/// can use any strategy to flip these bits but the default next()/allocate()
/// functions scan for the next available contiguous block of zero bits and
/// then wrap around when reaching the end of the array. Once a full iteration
/// of the array is made without finding satisfaction, an std::bad_alloc is
/// thrown.
///
struct ircd::allocator::state
{
	using word_t                                 = unsigned long long;
	using size_type                              = std::size_t;

	size_t size                                  { 0                                               };
	word_t *avail                                { nullptr                                         };
	size_t last                                  { 0                                               };

	static uint byte(const uint &i)              { return i / (sizeof(word_t) * 8);                }
	static uint bit(const uint &i)               { return i % (sizeof(word_t) * 8);                }
	static word_t mask(const uint &pos)          { return word_t(1) << bit(pos);                   }

	bool test(const uint &pos) const             { return avail[byte(pos)] & mask(pos);            }
	void bts(const uint &pos)                    { avail[byte(pos)] |= mask(pos);                  }
	void btc(const uint &pos)                    { avail[byte(pos)] &= ~mask(pos);                 }
	uint next(const size_t &n) const;

  public:
	bool available(const size_t &n = 1) const;
	void deallocate(const uint &p, const size_t &n);
	uint allocate(std::nothrow_t, const size_t &n, const uint &hint = -1);
	uint allocate(const size_t &n, const uint &hint = -1);

	state(const size_t &size = 0,
	      word_t *const &avail = nullptr)
	:size{size}
	,avail{avail}
	,last{0}
	{}
};

/// The fixed allocator creates a block of data with a size known at compile-
/// time. This structure itself is the state object for the actual allocator
/// instance used in the container. Create an instance of this structure,
/// perhaps on your stack. Then specify the ircd::allocator::fixed::allocator
/// in the template for the container. Then pass a reference to the state
/// object as an argument to the container when constructing. STL containers
/// have an overloaded constructor for this when specializing the allocator
/// template as we are here.
///
template<class T,
         size_t MAX>
struct ircd::allocator::fixed
:state
{
	struct allocator;
	using value = std::aligned_storage<sizeof(T), alignof(T)>;

	std::array<word_t, MAX / 8> avail {{0}};
	std::array<typename value::type, MAX> buf;

  public:
	bool in_range(const T *const &ptr) const
	{
		const auto base(reinterpret_cast<const T *>(buf.data()));
		return ptr >= base && ptr < base + MAX;
	}

	allocator operator()();
	operator allocator();

	fixed()
	:state{MAX, avail.data()}
	{}
};

/// The actual allocator template as used by the container.
///
/// This has to be a very light, small and copyable object which cannot hold
/// our actual memory or state (lest we just use dynamic allocation for that!)
/// which means we have to pass this a reference to our ircd::allocator::fixed
/// instance. We can do that through the container's custom-allocator overload
/// at its construction.
///
template<class T,
         size_t SIZE>
struct ircd::allocator::fixed<T, SIZE>::allocator
{
	using value_type         = T;
	using pointer            = T *;
	using const_pointer      = const T *;
	using reference          = T &;
	using const_reference    = const T &;
	using size_type          = std::size_t;
	using difference_type    = std::ptrdiff_t;

	fixed *s;

  public:
	template<class U> struct rebind
	{
		using other = typename fixed<U, SIZE>::allocator;
	};

	size_type max_size() const
	{
		return SIZE;
	}

	auto address(reference x) const
	{
		return &x;
	}

	auto address(const_reference x) const
	{
		return &x;
	}

	pointer allocate(std::nothrow_t, const size_type &n, const const_pointer &hint = nullptr)
	{
		const auto base(reinterpret_cast<pointer>(s->buf.data()));
		const uint hintpos(hint? hint - base : -1);
		const pointer ret(base + s->state::allocate(std::nothrow, n, hintpos));
		return s->in_range(ret)? ret : nullptr;
	}

	pointer allocate(const size_type &n, const const_pointer &hint = nullptr)
	{
		const auto base(reinterpret_cast<pointer>(s->buf.data()));
		const uint hintpos(hint? hint - base : -1);
		return base + s->state::allocate(n, hintpos);
	}

	void deallocate(const pointer &p, const size_type &n)
	{
		const auto base(reinterpret_cast<pointer>(s->buf.data()));
		s->state::deallocate(p - base, n);
	}

	template<class U,
	         size_t OTHER_SIZE = SIZE>
	allocator(const typename fixed<U, OTHER_SIZE>::allocator &s) noexcept
	:s{reinterpret_cast<fixed<T, SIZE> *>(s.s)}
	{
		static_assert(OTHER_SIZE == SIZE);
	}

	allocator(fixed &s) noexcept
	:s{&s}
	{}

	allocator(allocator &&) = default;
	allocator(const allocator &) = default;

	friend bool operator==(const allocator &a, const allocator &b)
	{
		return &a == &b;
	}

	friend bool operator!=(const allocator &a, const allocator &b)
	{
		return &a == &b;
	}
};

template<class T,
         size_t SIZE>
typename ircd::allocator::fixed<T, SIZE>::allocator
ircd::allocator::fixed<T, SIZE>::operator()()
{
	return ircd::allocator::fixed<T, SIZE>::allocator(*this);
}

template<class T,
         size_t SIZE>
ircd::allocator::fixed<T, SIZE>::operator
allocator()
{
	return ircd::allocator::fixed<T, SIZE>::allocator(*this);
}

/// The dynamic allocator provides a pool of a fixed size known at runtime.
///
/// This allocator conducts a single new and delete for a pool allowing an STL
/// container to operate without interacting with the rest of the system and
/// without fragmentation. This is not as useful as the allocator::fixed in
/// practice as the standard allocator is as good as this in many cases. This
/// is still available as an analog to the fixed allocator in this suite.
///
template<class T>
struct ircd::allocator::dynamic
:state
{
	struct allocator;

	size_t head_size, data_size;
	std::unique_ptr<uint8_t[]> arena;
	T *buf;

  public:
	allocator operator()();
	operator allocator();

	dynamic(const size_t &size)
	:state{size}
	,head_size{size / 8}
	,data_size{sizeof(T) * size + 16}
	,arena
	{
		new __attribute__((aligned(16))) uint8_t[head_size + data_size]
	}
	,buf
	{
		reinterpret_cast<T *>(arena.get() + head_size + (head_size % 16))
	}
	{
		state::avail = reinterpret_cast<word_t *>(arena.get());
	}
};

/// The actual template passed to containers for using the dynamic allocator.
///
/// See the notes for ircd::allocator::fixed::allocator for details.
///
template<class T>
struct ircd::allocator::dynamic<T>::allocator
{
	using value_type         = T;
	using pointer            = T *;
	using const_pointer      = const T *;
	using reference          = T &;
	using const_reference    = const T &;
	using size_type          = std::size_t;
	using difference_type    = std::ptrdiff_t;

	dynamic *s;

  public:
	template<class U> struct rebind
	{
		using other = typename dynamic<U>::allocator;
	};

	size_type max_size() const                   { return s->size;                                 }
	auto address(reference x) const              { return &x;                                      }
	auto address(const_reference x) const        { return &x;                                      }

	pointer allocate(const size_type &n, const const_pointer &hint = nullptr)
	{
		const uint hintpos(hint? hint - s->buf : -1);
		return s->buf + s->state::allocate(n, hintpos);
	}

	void deallocate(const pointer &p, const size_type &n)
	{
		const uint pos(p - s->buf);
		s->state::deallocate(pos, n);
	}

	template<class U>
	allocator(const typename dynamic<U>::allocator &) noexcept
	:s{reinterpret_cast<dynamic *>(s.s)}
	{}

	allocator(dynamic &s) noexcept
	:s{&s}
	{}

	allocator(allocator &&) = default;
	allocator(const allocator &) = default;

	friend bool operator==(const allocator &a, const allocator &b)
	{
		return &a == &b;
	}

	friend bool operator!=(const allocator &a, const allocator &b)
	{
		return &a == &b;
	}
};

template<class T>
typename ircd::allocator::dynamic<T>::allocator
ircd::allocator::dynamic<T>::operator()()
{
	return ircd::allocator::dynamic<T>::allocator(*this);
}

template<class T>
ircd::allocator::dynamic<T>::operator
allocator()
{
	return ircd::allocator::dynamic<T>::allocator(*this);
}

/// Allows elements of an STL container to be manually handled by the user.
///
/// C library containers usually allow the user to manually construct a node
/// and then insert it and remove it from the container. With STL containers
/// we can use devices like allocator::fixed, but what if we don't want to have
/// a bound on the allocator's size either at compile time or at runtime? What
/// if we simply want to manually handle the container's elements, like on the
/// stack, and in different frames, and then manipulate the container -- or
/// even better and safer: have the elements add and remove themselves while
/// storing the container's node data too?
///
/// This device helps the user achieve that by simply providing a variable
/// set by the user indicating where the 'next' block of memory is when the
/// container requests it. Whether the container is requesting memory which
/// should be fulfilled by that 'next' block must be ensured and asserted by
/// the user, but this is likely the case.
///
template<class T>
struct ircd::allocator::node
{
	struct allocator;
	struct monotonic;

	T *next {nullptr};

	node() = default;
};

/// The actual template passed to containers for using the allocator.
///
/// See the notes for ircd::allocator::fixed::allocator for details.
///
template<class T>
struct ircd::allocator::node<T>::allocator
{
	using value_type         = T;
	using pointer            = T *;
	using const_pointer      = const T *;
	using reference          = T &;
	using const_reference    = const T &;
	using size_type          = std::size_t;
	using difference_type    = std::ptrdiff_t;

	node *s;

  public:
	template<class U> struct rebind
	{
		using other = typename node<U>::allocator;
	};

	size_type max_size() const                   { return std::numeric_limits<size_t>::max();      }
	auto address(reference x) const              { return &x;                                      }
	auto address(const_reference x) const        { return &x;                                      }

	template<class U, class... args>
	void construct(U *p, args&&... a)
	{
		new (p) U(std::forward<args>(a)...);
	}

	void construct(pointer p, const_reference val)
	{
		new (p) T(val);
	}

	pointer allocate(const size_type &n, const const_pointer &hint = nullptr)
	{
		assert(n == 1);
		assert(hint == nullptr);
		assert(s->next != nullptr);
		return s->next;
	}

	void deallocate(const pointer &p, const size_type &n)
	{
		assert(n == 1);
	}

	template<class U>
	allocator(const typename node<U>::allocator &s) noexcept
	:s{reinterpret_cast<node *>(s.s)}
	{
	}

	template<class U>
	allocator(const U &s) noexcept
	:s{reinterpret_cast<node *>(s.s)}
	{
	}

	allocator(node &s) noexcept
	:s{&s}
	{
	}

	allocator() = default;
	allocator(allocator &&) noexcept = default;
	allocator(const allocator &) = default;

	friend bool operator==(const allocator &a, const allocator &b)
	{
		return &a == &b;
	}

	friend bool operator!=(const allocator &a, const allocator &b)
	{
		return &a == &b;
	}
};

/// The twolevel allocator uses both a fixed allocator (first level) and then
/// the standard allocator (second level) when the fixed allocator is exhausted.
/// This has the intent that the fixed allocator will mostly be used, but the
/// fallback to the standard allocator is seamlessly available for robustness.
template<class T,
         size_t L0_SIZE>
struct ircd::allocator::twolevel
{
	struct allocator;

	fixed<T, L0_SIZE> l0;
	std::allocator<T> l1;

  public:
	allocator operator()();
	operator allocator();

	twolevel() = default;
};

template<class T,
         size_t L0_SIZE>
struct ircd::allocator::twolevel<T, L0_SIZE>::allocator
{
	using value_type         = T;
	using pointer            = T *;
	using const_pointer      = const T *;
	using reference          = T &;
	using const_reference    = const T &;
	using size_type          = std::size_t;
	using difference_type    = std::ptrdiff_t;

	twolevel *s;

  public:
	template<class U,
	         size_t OTHER_L0_SIZE = L0_SIZE>
	struct rebind
	{
		using other = typename twolevel<U, OTHER_L0_SIZE>::allocator;
	};

	size_type max_size() const
	{
		return std::numeric_limits<size_type>::max();
	}

	auto address(reference x) const
	{
		return &x;
	}

	auto address(const_reference x) const
	{
		return &x;
	}

	pointer allocate(const size_type &n, const const_pointer &hint = nullptr)
	{
		assert(s);
		return
			s->l0.allocate(std::nothrow, n, hint)?:
			s->l1.allocate(n, hint);
	}

	void deallocate(const pointer &p, const size_type &n)
	{
		assert(s);
		if(likely(s->l0.in_range(p)))
			s->l0.deallocate(p, n);
		else
			s->l1.deallocate(p, n);
	}

	template<class U,
	         size_t OTHER_L0_SIZE = L0_SIZE>
	allocator(const typename twolevel<U, OTHER_L0_SIZE>::allocator &s) noexcept
	:s{reinterpret_cast<twolevel<T, L0_SIZE> *>(s.s)}
	{
		static_assert(OTHER_L0_SIZE == L0_SIZE);
	}

	allocator(twolevel &s) noexcept
	:s{&s}
	{}

	allocator(allocator &&) = default;
	allocator(const allocator &) = default;

	friend bool operator==(const allocator &a, const allocator &b)
	{
		return &a == &b;
	}

	friend bool operator!=(const allocator &a, const allocator &b)
	{
		return &a == &b;
	}
};

template<class T,
         size_t L0_SIZE>
typename ircd::allocator::twolevel<T, L0_SIZE>::allocator
ircd::allocator::twolevel<T, L0_SIZE>::operator()()
{
	return ircd::allocator::twolevel<T, L0_SIZE>::allocator(*this);
}

template<class T,
         size_t L0_SIZE>
ircd::allocator::twolevel<T, L0_SIZE>::operator
allocator()
{
	return ircd::allocator::twolevel<T, L0_SIZE>::allocator(*this);
}
