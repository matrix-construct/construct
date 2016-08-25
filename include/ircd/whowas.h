/*
 *  ircd-ratbox: A slightly useful ircd.
 *  whowas.h: Header for the whowas functions.
 *
 *  Copyright (C) 1990 Jarkko Oikarinen and University of Oulu, Co Center
 *  Copyright (C) 1996-2002 Hybrid Development Team
 *  Copyright (C) 2002-2004 ircd-ratbox development team
 *  Copyright (C) 2016 Charybdis Development Team
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
#define HAVE_IRCD_WHOWAS_H

#ifdef __cplusplus
namespace ircd    {
namespace whowas  {

using client::client;
using id_t = uint64_t;

/* lets speed this up... also removed away information. *tough*
 * - Dianora
 */
struct whowas
{
	enum flag
	{
		IP_SPOOFING  = 0x01,
		DYNSPOOF     = 0x02,
	};

	id_t wwid;                                    // Unique whowas index ID 
	client *online;                               // Pointer to online client or nullptr
	time_t logoff;
	std::shared_ptr<cache::serv::entry> scache;
	char name[NICKLEN + 1];
	char username[USERLEN + 1];
	char hostname[HOSTLEN + 1];
	char sockhost[HOSTIPLEN + 1];
	char realname[REALLEN + 1];
	char suser[NICKLEN + 1];
	enum flag flag;

	whowas(client &client);
};

// Get a full history for nickname (may contain different users!)
std::vector<std::shared_ptr<whowas>> history(const std::string &name, const time_t &limit = 0, const bool &online = false);

// Get a full history for this unique whowas ID. This is effectively similar to
// a lookup by a client pointer address; allocators may reuse the same address,
// so this ID is used as the index instead.
std::vector<std::shared_ptr<whowas>> history(const id_t &wwid);

// Get a full history for this client (must be online);
std::vector<std::shared_ptr<whowas>> history(const client &);

// Add whowas information about client *before* a nick change or logoff.
void add(client &);

// Notify this subsystem the client pointer is about to be invalidated (does not call add())
void off(client &);

// Util
void memory_usage(size_t *const &count, size_t *const &memused);
void set_size(const size_t &max);
void init();

}      // namespace whowas
}      // namespace ircd
#endif // __cplusplus
