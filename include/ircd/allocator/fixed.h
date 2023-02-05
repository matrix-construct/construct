// Matrix Construct
//
// Copyright (C) Matrix Construct Developers, Authors & Contributors
// Copyright (C) 2016-2023 Jason Volk <jason@zemos.net>
//
// Permission to use, copy, modify, and/or distribute this software for any
// purpose with or without fee is hereby granted, provided that the above
// copyright notice and this permission notice is present in all copies. The
// full license for this software is available in the LICENSE file.

#pragma once
#define HAVE_IRCD_ALLOCATOR_FIXED_H

namespace ircd::allocator
{
	template<class T = char,
	         size_t = 512>
	struct fixed;
}

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
	{
		static_cast<state &>(*this) =
		{
			MAX, avail.data()
		};
	}
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

	pointer
	__attribute__((malloc, warn_unused_result))
	allocate(std::nothrow_t, const size_type &n, const const_pointer &hint = nullptr)
	{
		const auto base(reinterpret_cast<pointer>(s->buf.data()));
		const uint hintpos(hint? uint(hint - base) : uint(-1));
		const pointer ret(base + s->state::allocate(std::nothrow, n, hintpos));
		return s->in_range(ret)? ret : nullptr;
	}

	pointer
	__attribute__((malloc, returns_nonnull, warn_unused_result))
	allocate(const size_type &n, const const_pointer &hint = nullptr)
	{
		const auto base(reinterpret_cast<pointer>(s->buf.data()));
		const uint hintpos(hint? uint(hint - base) : uint(-1));
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
inline typename ircd::allocator::fixed<T, SIZE>::allocator
ircd::allocator::fixed<T, SIZE>::operator()()
{
	return ircd::allocator::fixed<T, SIZE>::allocator(*this);
}

template<class T,
         size_t SIZE>
inline ircd::allocator::fixed<T, SIZE>::operator
allocator()
{
	return ircd::allocator::fixed<T, SIZE>::allocator(*this);
}
