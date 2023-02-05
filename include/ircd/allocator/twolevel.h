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
#define HAVE_IRCD_ALLOCATOR_TWOLEVEL_H

namespace ircd::allocator
{
	template<class T = char,
	         size_t L0_SIZE = 512>
	struct twolevel;
}

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

	pointer
	__attribute__((malloc, returns_nonnull, warn_unused_result))
	allocate(const size_type &n, const const_pointer &hint = nullptr)
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
inline typename ircd::allocator::twolevel<T, L0_SIZE>::allocator
ircd::allocator::twolevel<T, L0_SIZE>::operator()()
{
	return ircd::allocator::twolevel<T, L0_SIZE>::allocator(*this);
}

template<class T,
         size_t L0_SIZE>
inline ircd::allocator::twolevel<T, L0_SIZE>::operator
allocator()
{
	return ircd::allocator::twolevel<T, L0_SIZE>::allocator(*this);
}
