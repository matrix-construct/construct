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
#define HAVE_IRCD_ALLOCATOR_CALLBACK_H

namespace ircd::allocator
{
	template<class T = void>
	struct callback;
}

/// The callback allocator is a shell around the pre-c++17/20 boilerplate
/// jumble for allocator template creation. This is an alternative to virtual
/// functions to accomplish the same thing here. Implement the principal
/// allocate and deallocate functions and maintain an instance of
/// allocator::callback with them somewhere.
template<class T>
struct ircd::allocator::callback
{
	struct allocator;

  public:
	using allocate_callback = std::function<T *(const size_t &, const T *const &)>;
	using deallocate_callback = std::function<void (T *const &, const size_t &)>;

	allocate_callback ac;
	deallocate_callback dc;

	allocator operator()();
	operator allocator();

	callback(allocate_callback ac, deallocate_callback dc)
	:ac{std::move(ac)}
	,dc{std::move(dc)}
	{}
};

template<class T>
struct ircd::allocator::callback<T>::allocator
{
	using value_type         = T;
	using size_type          = std::size_t;
	using difference_type    = std::ptrdiff_t;

	using is_always_equal = std::true_type;
	using propagate_on_container_move_assignment = std::true_type;

	callback *s;

  public:
	template<class U> struct rebind
	{
		typedef ircd::allocator::callback<T>::allocator other;
	};

	T *
	__attribute__((malloc, returns_nonnull, warn_unused_result))
	allocate(const size_type n, const T *const hint = nullptr)
	{
		assert(s && s->ac);
		return s->ac(n, hint);
	}

	void deallocate(T *const p, const size_type n = 1)
	{
		assert(s && s->dc);
		return s->dc(p, n);
	}

	template<class U>
	allocator(const typename ircd::allocator::callback<U>::allocator &s) noexcept
	:s{s.s}
	{}

	allocator(callback &s) noexcept
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
inline typename ircd::allocator::callback<T>::allocator
ircd::allocator::callback<T>::operator()()
{
	return ircd::allocator::callback<T>::allocator(*this);
}

template<class T>
inline ircd::allocator::callback<T>::operator
allocator()
{
	return ircd::allocator::callback<T>::allocator(*this);
}
