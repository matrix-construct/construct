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
#define HAVE_IRCD_UTIL_CALLBACKS_H

namespace ircd::util
{
	/// The purpose of callbacks is simply explained with an analogy out of
	/// browser-javascript land: it is window.onload.addEventListener() in lieu
	/// of assigning window.onload = function(). This is a list of multiple
	/// callbacks listening for invocation. The listener is responsible for
	/// both adding and removing itself using the std list interface.
	///
	/// The template provides an option for whether exceptions should propagate
	/// to the caller. If they propagate, all listeners after the exception won't
	/// be invoked.
	/// 
	template<class prototype,
	         bool exceptions = true>
	struct callbacks;

	template<class prototype>
	struct callbacks<prototype, true>;

	template<class prototype>
	struct callbacks<prototype, false>;
}

template<class prototype>
struct ircd::util::callbacks<prototype, true>
:std::list<std::function<prototype>>
{
	template<class... args>
	void operator()(args&&... a) const
	{
		for(const auto &func : *this)
			func(std::forward<args>(a)...);
	}

	callbacks() = default;
	callbacks(callbacks &&) = default;
	callbacks(const callbacks &) = delete;
	callbacks &operator=(callbacks &&) = default;
	callbacks &operator=(const callbacks &) = delete;
};

template<class prototype>
struct ircd::util::callbacks<prototype, false>
:std::list<std::function<prototype>>
{
	template<class... args>
	void operator()(args&&... a) const
	{
		for(const auto &func : *this) try
		{
			func(std::forward<args>(a)...);
		}
		catch(const std::exception &e)
		{
			// Logging isn't available in ircd::util;
			// just silently drop and continue.
		}
	}

	callbacks() = default;
	callbacks(callbacks &&) = default;
	callbacks(const callbacks &) = delete;
	callbacks &operator=(callbacks &&) = default;
	callbacks &operator=(const callbacks &) = delete;
};
