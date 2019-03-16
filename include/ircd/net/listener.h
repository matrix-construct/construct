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
	struct acceptor;

	std::ostream &operator<<(std::ostream &s, const listener &);
	std::ostream &operator<<(std::ostream &s, const acceptor &);

	extern conf::item<bool> listen;

	std::string cipher_list(const acceptor &);
	json::object config(const acceptor &);
	string_view name(const acceptor &);

	bool start(acceptor &);
	bool stop(acceptor &);
}

struct ircd::net::listener
{
	using callback = std::function<void (listener &, const std::shared_ptr<socket> &)>;
	using proffer = std::function<bool (listener &, const ipport &)>;

	IRCD_EXCEPTION(net::error, error)

  private:
	std::shared_ptr<net::acceptor> acceptor;

  public:
	operator const net::acceptor &() const;
	operator net::acceptor &();

	explicit operator json::object() const;
	string_view name() const;

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
