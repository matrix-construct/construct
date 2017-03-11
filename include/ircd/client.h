/*
 *  charybdis: A useful ircd.
 *  client.h: The ircd client header.
 *
 *  Copyright (C) 1990 Jarkko Oikarinen and University of Oulu, Co Center
 *  Copyright (C) 1996-2002 Hybrid Development Team
 *  Copyright (C) 2002-2004 ircd-ratbox development team
 *  Copyright (C) 2005 William Pitcock and Jilles Tjoelker
 *  Copyright (C) 2005-2016 Charybdis Development Team
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307
 *  USA
 */

#pragma once
#define HAVE_IRCD_CLIENT_H

namespace ircd {

IRCD_EXCEPTION(ircd::error, client_error)
IRCD_EXCEPTION(client_error, broken_pipe)
IRCD_EXCEPTION(client_error, disconnected)

struct socket;
using host_port_pair = std::pair<std::string, uint16_t>;
using host_port = IRCD_WEAK_T(host_port_pair);
std::string string(const host_port &);

struct client
{
	struct init;
	using list = std::list<client *>;

	static list clients;

	const char *const type;
	list::const_iterator clit;
	std::shared_ptr<socket> sock;

	virtual bool handle();
	bool main() noexcept;

  public:
	explicit client(std::shared_ptr<socket>);
	explicit client(const host_port &, const seconds &timeout = 5s);
	client();
	client(client &&) = delete;
	client(const client &) = delete;
	client &operator=(client &&) = delete;
	client &operator=(const client &) = delete;
	virtual ~client() noexcept;
};

host_port remote_address(const client &);
host_port local_address(const client &);

// Creates a client.
std::shared_ptr<client> add_client(std::shared_ptr<socket>);

} // namespace ircd

namespace ircd {
namespace http {

struct client
:ircd::client
{
	virtual bool handle() override;

	using ircd::client::client;
};

} // namespace ircd
} // namespace http

namespace ircd {

struct client::init
{
	init();
	~init() noexcept;
};

} // namespace ircd
