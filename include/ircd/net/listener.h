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
#define HAVE_IRCD_NET_LISTENER_H

namespace ircd::net
{
	struct listener;
	struct listener_udp;
}

struct ircd::net::listener
{
	struct acceptor;
	using callback = std::function<void (const std::shared_ptr<socket> &)>;

	IRCD_EXCEPTION(net::error, error)

  private:
	std::shared_ptr<struct acceptor> acceptor;

  public:
	listener(const string_view &name,
	         const json::object &options,
	         callback);

	explicit
	listener(const string_view &name,
	         const std::string &options,
	         callback);

	~listener() noexcept;
};

struct ircd::net::listener_udp
{
	struct acceptor;
	struct datagram;
	enum flag :uint;

	IRCD_EXCEPTION(net::error, error)

  private:
	std::unique_ptr<struct acceptor> acceptor;

  public:
	datagram &operator()(datagram &);

	listener_udp(const string_view &name,
	             const json::object &options);

	explicit
	listener_udp(const string_view &name,
	             const std::string &options);

	~listener_udp() noexcept;
};

struct ircd::net::listener_udp::datagram
{
	mutable_buffer buf;
	ipport remote;
	enum flag flag {(enum flag)0};
	vector_view<mutable_buffer> bufs { &buf, 1 };

	datagram() = default;
};

enum ircd::net::listener_udp::flag
:uint
{
	PEEK  = 0x01,
};
