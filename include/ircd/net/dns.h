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
#define HAVE_IRCD_NET_DNS_H

namespace ircd::net
{
	struct dns extern dns;
}

/// DNS resolution suite.
///
/// This is a singleton class; public usage is to make calls on the singleton
/// object like `ircd::net::dns()` etc.
///
struct ircd::net::dns
{
	struct resolver;

	struct resolver static *resolver;

  public:
	enum flag :uint;

	using callback_one = std::function<void (std::exception_ptr, const ipport &)>;
	using callback_many = std::function<void (std::exception_ptr, vector_view<const ipport>)>;
	using callback_reverse = std::function<void (std::exception_ptr, const string_view &)>;

	// Callback-based interface
	void operator()(const ipport &, callback_reverse);
	void operator()(const hostport &, callback_many);
	void operator()(const hostport &, callback_one);

	// Future-based interface
	ctx::future<ipport> operator()(const hostport &);
	ctx::future<std::string> operator()(const ipport &);
};

enum ircd::net::dns::flag
:uint
{

};
