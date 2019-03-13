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
#define HAVE_IRCD_NET_ACCEPTOR_H

// This file is not included with the IRCd standard include stack because
// it requires symbols we can't forward declare without boost headers. It
// is part of the <ircd/asio.h> stack which can be included in your
// definition file if you need low level access to this acceptor API.

struct ircd::net::acceptor
:std::enable_shared_from_this<struct ircd::net::acceptor>
{
	using error_code = boost::system::error_code;
	using callback = listener::callback;
	using proffer = listener::proffer;

	IRCD_EXCEPTION(listener::error, error)

	static log::log log;
	static conf::item<milliseconds> timeout;
	static conf::item<std::string> ssl_cipher_list;
	static conf::item<std::string> ssl_cipher_blacklist;

	net::listener *listener_;
	std::string name;
	std::string opts;
	size_t backlog;
	listener::callback cb;
	listener::proffer pcb;
	asio::ssl::context ssl;
	ip::tcp::endpoint ep;
	ip::tcp::acceptor a;
	size_t accepting {0};
	size_t handshaking {0};
	bool interrupting {false};
	bool handle_set {false};
	ctx::dock joining;

	void configure(const json::object &opts);

	// Handshake stack
	void check_handshake_error(const error_code &ec, socket &);
	void handshake(const error_code &ec, std::shared_ptr<socket>, std::weak_ptr<acceptor>) noexcept;

	// Acceptance stack
	bool check_accept_error(const error_code &ec, socket &);
	void accept(const error_code &ec, std::shared_ptr<socket>, std::weak_ptr<acceptor>) noexcept;

	// Accept next
	bool set_handle();

	// Acceptor shutdown
	bool interrupt() noexcept;
	void join() noexcept;

	acceptor(net::listener &,
	         const string_view &name,
	         const json::object &opts,
	         listener::callback,
	         listener::proffer);

	~acceptor() noexcept;
};
