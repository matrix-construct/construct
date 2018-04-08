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
	template<class T = char> struct dynamic;
	template<class T = char, size_t = 512> struct fixed;
	template<class T> struct node;
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
	void deallocate(const uint &p, const size_t &n);
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
         size_t max>
struct ircd::allocator::fixed
:state
{
	struct allocator;
	using value = std::aligned_storage<sizeof(T), alignof(T)>;

	std::array<word_t, max / 8> avail {{0}};
	std::array<typename value::type, max> buf;

  public:
	allocator operator()();
	operator allocator();

	fixed()
	:state{max, avail.data()}
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
         size_t size>
struct ircd::allocator::fixed<T, size>::allocator
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
	template<class U, size_t S> struct rebind
	{
		using other = typename fixed<U, S>::allocator;
	};

	size_type max_size() const                   { return size;                                    }
	auto address(reference x) const              { return &x;                                      }
	auto address(const_reference x) const        { return &x;                                      }

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

	template<class U, size_t S>
	allocator(const typename fixed<U, S>::allocator &) noexcept
	:s{reinterpret_cast<fixed *>(s.s)}
	{}

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
         size_t size>
typename ircd::allocator::fixed<T, size>::allocator
ircd::allocator::fixed<T, size>::operator()()
{
	return { *this };
}

template<class T,
         size_t size>
ircd::allocator::fixed<T, size>::operator
allocator()
{
	return { *this };
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
	return { *this };
}

template<class T>
ircd::allocator::dynamic<T>::operator
allocator()
{
	return { *this };
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

inline void
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

inline uint
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

inline uint
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
