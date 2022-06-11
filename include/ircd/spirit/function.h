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
#define HAVE_IRCD_SPIRIT_FUNCTION_H

// This all prevents spirit grammar rules from generating a very large and/or
// deep call-graph where rules compose together with indirect calls and thunks
// through boost::function -- this is higly inefficient as the grammar's logic
// ends up being a fraction of the generated code and the rest is invocation
// related overhead since lots of optimizations become inhibited.

// Spirit uses `boost::function` by default, but since it's in boost:: it
// declares as `function`. We can immediately do a lot better by simply
// dropping in `std::function` as said `function` instead. However, this does
// not go far enough for most cases so we maximize optimization with a bespoke
// function object.

// Our optimized function object is built in ircd::spirit so as to not
// interpose itself by default; see below for controlling conditions.
namespace ircd::spirit
{
	template<class prototype>
	class function;

	template<class R,
	         class... args>
	class function<R (args...)>;
}

// Fuse-box controlling spirit::qi parsers.
namespace boost::spirit::qi
{
	#if defined(__clang__)
		using ircd::spirit::function;
	#else
		using std::function;
	#endif
}

// Fusebox controlling spirit::karma generators.
namespace boost::spirit::karma
{
	#if defined(__clang__)
		using ircd::spirit::function;
	#else
		using std::function;
	#endif
}

/// Internal optimized function object.
template<class R,
         class... args>
class [[clang::internal_linkage]]
ircd::spirit::function<R (args...)>
{
	R (*f)(void *, args...)
	{
		nullptr
	};

	void *t
	{
		nullptr
	};

	template<class T>
	static R handler(void *, args...);

  public:
	explicit operator bool() const noexcept;

	R operator()(args...) const;

	template<class T>
	function &operator=(T *const t) & noexcept;

	template<class T>
	function(T *const t) noexcept;

	function(const function &, void *const &) noexcept;
	function() = default;
	function(function &&) = default;
	function(const function &) = delete;
	function &operator=(function &&) = default;
	function &operator=(const function &) = delete;
	~function() noexcept = default;
};

template<class R,
         class... args>
inline
ircd::spirit::function<R (args...)>::function(const function &f,
                                              void *const &t)
noexcept
:f{f.f}
,t{t}
{}

template<class R,
         class... args>
template<class T>
inline
ircd::spirit::function<R (args...)>::function(T *const t)
noexcept
:f{this->handler<T>}
,t{t}
{}

template<class R,
         class... args>
template<class T>
inline ircd::spirit::function<R (args...)> &
ircd::spirit::function<R (args...)>::operator=(T *const t)
& noexcept
{
	this->f = this->handler<T>;
	this->t = t;
	return *this;
}

template<class R,
         class... args>
template<class T>
[[gnu::section("text.ircd.spirit")]]
inline R
ircd::spirit::function<R (args...)>::handler(void *const t,
                                             args... a)
{
	assert(t);
	return static_cast<T *>(t)->operator()(a...);
}

template<class R,
         class... args>
[[gnu::always_inline]]
inline R
ircd::spirit::function<R (args...)>::operator()(args... a)
const
{
	assert(this->f);
	return this->f(this->t, a...);
}

template<class R,
         class... args>
inline ircd::spirit::function<R (args...)>::operator
bool()
const noexcept
{
	assert(!f || (f && t));
	return f;
}
