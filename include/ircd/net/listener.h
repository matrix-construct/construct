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

	extern conf::item<bool> listen;
}

struct ircd::net::listener
{
	struct acceptor;
	using callback = std::function<void (listener &, const std::shared_ptr<socket> &)>;
	using proffer = std::function<bool (listener &, const ipport &)>;

	IRCD_EXCEPTION(net::error, error)

  private:
	std::shared_ptr<struct acceptor> acceptor;

  public:
	explicit operator json::object() const;
	string_view name() const;

	bool start();

	listener(const string_view &name,
	         const json::object &options,
	         callback,
	         proffer = {});

	explicit
	listener(const string_view &name,
	         const std::string &options,
	         callback,
	         proffer = {});

	~listener() noexcept;

	friend std::ostream &operator<<(std::ostream &s, const listener &);
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

	datagram() = default;
};

enum ircd::net::listener_udp::flag
:uint
{
	PEEK  = 0x01,
};
