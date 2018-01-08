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

namespace ircd
{
	struct client;

	const char *write(client &, const char *&start, const char *const &stop);
	char *read(client &, char *&start, char *const &stop);

	http::response::write_closure write_closure(client &);
	parse::read_closure read_closure(client &);

	std::shared_ptr<client> add_client(std::shared_ptr<socket>);  // Creates a client.
}

struct ircd::client
:std::enable_shared_from_this<client>
{
	struct init;

	using list = std::list<client *>;

	static list clients;

	unique_const_iterator<list> clit;
	std::shared_ptr<socket> sock;
	ircd::timer request_timer;

	bool handle(parse::buffer &pb);
	bool main() noexcept;

  public:
	client(std::shared_ptr<socket>);
	client(const hostport &, const seconds &timeout = 5s);
	client();
	client(client &&) = delete;
	client(const client &) = delete;
	client &operator=(client &&) = delete;
	client &operator=(const client &) = delete;
	virtual ~client() noexcept;

	friend ipport remote(const client &);
	friend ipport local(const client &);
};

struct ircd::client::init
{
	void interrupt();

	init();
	~init() noexcept;
};
