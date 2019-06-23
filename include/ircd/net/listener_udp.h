// Matrix Construct
//
// Copyright (C) Matrix Construct Developers, Authors & Contributors
// Copyright (C) 2016-2019 Jason Volk <jason@zemos.net>
//
// Permission to use, copy, modify, and/or distribute this software for any
// purpose with or without fee is hereby granted, provided that the above
// copyright notice and this permission notice is present in all copies. The
// full license for this software is available in the LICENSE file.

#pragma once
#define HAVE_IRCD_NET_LISTENER_UDP_H

namespace ircd::net
{
	struct listener_udp;
	struct acceptor_udp;

	string_view loghead(const mutable_buffer &, const acceptor_udp &);
	string_view loghead(const acceptor_udp &);
	std::ostream &operator<<(std::ostream &s, const listener_udp &);
	std::ostream &operator<<(std::ostream &s, const acceptor_udp &);
}

struct ircd::net::listener_udp
{
	struct datagram;
	enum flag :uint;

	IRCD_EXCEPTION(net::error, error)

  private:
	std::unique_ptr<net::acceptor_udp> acceptor;

  public:
	explicit operator json::object() const;
	string_view name() const;

	datagram &operator()(datagram &);

	listener_udp(const string_view &name,
	             const json::object &options);

	explicit
	listener_udp(const string_view &name,
	             const std::string &options);

	~listener_udp() noexcept;

	friend std::ostream &operator<<(std::ostream &s, const listener_udp &);
};

struct ircd::net::listener_udp::datagram
{
	union
	{
		const_buffer cbuf;
		mutable_buffer mbuf;
	};

	union
	{
		vector_view<const_buffer> cbufs;
		vector_view<mutable_buffer> mbufs;
	};

	ipport remote;
	enum flag flag {(enum flag)0};

	datagram(const const_buffer &buf,
	         const ipport &remote,
	         const enum flag &flag = (enum flag)0);

	datagram(const mutable_buffer &buf,
	         const enum flag &flag = (enum flag)0);

	datagram() {}
};

enum ircd::net::listener_udp::flag
:uint
{
	PEEK  = 0x01,
};
