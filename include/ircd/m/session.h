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
#define HAVE_IRCD_M_SESSION_H

namespace ircd::m
{
	struct session;
}

struct ircd::m::session
{
	net::remote remote;
	std::string access_token;

	server::request operator()(const server::out &out, const server::in &in) const;
	template<class... args> server::request operator()(const mutable_buffer &buf, args&&...) const;

	session(const net::remote &remote)
	:remote{remote}
	{}

	session(const net::hostport &remote)
	:remote{remote}
	{}

	session() = default;
};

template<class... args>
ircd::server::request
ircd::m::session::operator()(const mutable_buffer &buf,
                             args&&... a)
const
{
	m::request request
	{
		std::forward<args>(a)...
	};

	if(!defined(json::get<"destination"_>(request)))
		json::get<"destination"_>(request) = remote.hostname;

	if(!defined(json::get<"origin"_>(request)))
		json::get<"origin"_>(request) = my_host();

	const auto head
	{
		request(buf)
	};

	const server::out out
	{
		head
	};

	const auto in_max
	{
		std::max(ssize_t(size(buf) - size(head)), ssize_t(0))
	};

	assert(in_max >= ssize_t(size(buf) / 2));
	const server::in in
	{
		{ data(buf) + size(head), size_t(in_max) }
	};

	return operator()(out, in);
}

inline ircd::server::request
ircd::m::session::operator()(const server::out &out,
                             const server::in &in)
const
{
	return
	{
		remote, out, in
	};
}
