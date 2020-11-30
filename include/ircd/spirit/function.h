// The Construct
//
// Copyright (C) The Construct Developers, Authors & Contributors
// Copyright (C) 2016-2020 Jason Volk <jason@zemos.net>
//
// Permission to use, copy, modify, and/or distribute this software for any
// purpose with or without fee is hereby granted, provided that the above
// copyright notice and this permission notice is present in all copies. The
// full license for this software is available in the LICENSE file.

#pragma once
#define HAVE_IRCD_SPIRIT_BOOST_FUNCTION_H

#if !defined(__clang__)
#pragma GCC visibility push (internal)
namespace boost::spirit
{
	// Spirit uses `boost::function` by default, but since it's in boost:: it
	// simply declares as `function`. We can do a lot better by dropping in
	// `std::function` instead.
	using std::function;
}
#pragma GCC visibility pop
#endif

#if defined(__clang__)
#pragma GCC visibility push (internal)
namespace boost::spirit
{
	template<class prototype>
	struct function;

	template<class R,
	         class... args>
	struct function<R (args...)>;
}

template<class R,
         class... args>
class [[clang::internal_linkage]]
boost::spirit::function<R (args...)>
{
	size_t s
	{
		0UL
	};

	R (*f)(const void *const &, args...)
	{
		nullptr
	};

	std::unique_ptr<char, decltype(&std::free)> o
	{
		nullptr, std::free
	};

  public:
	explicit operator bool() const noexcept
	{
		assert(f);
		assert(o);
		return true;
	}

	decltype(auto) operator()(args&&...) const;
	decltype(auto) operator()(args&&...);

	template<class binder>
	[[clang::internal_linkage]]
	function &operator=(binder) noexcept;

	function() = default;
	function(const function &);
	function(function &&) noexcept = default;
	~function() noexcept = default;
};

template<class R,
         class... args>
boost::spirit::function<R (args...)>::function(const function &o)
:s{o.s}
,f{o.f}
,o
{
	this->s?
		ircd::aligned_alloc(64, this->s):
		std::unique_ptr<char, decltype(&std::free)>
		{
			nullptr, std::free
		}
}
{
	memcpy(this->o.get(), o.o.get(), this->s);
}

template<class R,
         class... args>
template<class binder>
[[clang::reinitializes]]
boost::spirit::function<R (args...)> &
boost::spirit::function<R (args...)>::operator=(binder o)
noexcept
{
	constexpr auto alignment
	{
		std::max(std::alignment_of<binder>::value, 64UL)
	};

	this->s = sizeof(binder);
	this->o = ircd::aligned_alloc(alignment, this->s);
	this->f = [](const void *const &o, args&&... a)
	{
		const auto &object
		{
			*reinterpret_cast<const binder *>(o)
		};

		return object(std::forward<args>(a)...);
	};

	new (this->o.get()) binder
	{
		std::move(o)
	};

	return *this;
}

template<class R,
         class... args>
[[gnu::always_inline, gnu::artificial]]
inline decltype(auto)
boost::spirit::function<R (args...)>::operator()(args&&... a)
{
	assert(bool(*this));
	const auto o
	{
		std::addressof(*this->o)
	};

	return this->f(o, std::forward<args>(a)...);
}

template<class R,
         class... args>
[[gnu::always_inline, gnu::artificial]]
inline decltype(auto)
boost::spirit::function<R (args...)>::operator()(args&&... a)
const
{
	assert(bool(*this));
	const auto o
	{
		std::addressof(*this->o)
	};

	return this->f(o, std::forward<args>(a)...);
}

#pragma GCC visibility pop
#endif
