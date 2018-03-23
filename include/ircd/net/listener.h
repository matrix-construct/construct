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
}

struct ircd::net::listener
{
	struct acceptor;

	IRCD_EXCEPTION(ircd::error, error)

  private:
	std::shared_ptr<struct acceptor> acceptor;

  public:
	explicit listener(const string_view &name, const std::string &options);
	listener(const string_view &name, const json::object &options);
	~listener() noexcept;
};
