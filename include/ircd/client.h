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

#ifdef __cplusplus
namespace ircd {

struct sock;
struct client;

std::shared_ptr<client> shared_from(client &);
std::weak_ptr<client> weak_from(client &);

// Client socket addressing
bool has_socket(const client &);
const sock &socket(const client &);
sock &socket(client &);

using ip_port_pair = std::pair<std::string, uint16_t>;
using ip_port = IRCD_WEAK_T(ip_port_pair);
ip_port remote_address(const client &);
ip_port local_address(const client &);
std::string string(const ip_port &);

enum class dc
{
	RST,          // hardest disconnect
	FIN,          // graceful shutdown both directions
	FIN_SEND,     // graceful shutdown send side
	FIN_RECV,     // graceful shutdown recv side
};


bool connected(const client &) noexcept;
bool disconnect(std::nothrow_t, client &, const dc & = dc::FIN) noexcept;
void disconnect(client &, const dc & = dc::FIN);
void recv_cancel(client &);
void recv_next(client &, const std::chrono::milliseconds &timeout);
void recv_next(client &);

// Destroys a client. This only removes the client from the clients list,
// and may result in a destruction and disconnect, or it may not.
void finished(client &);

// Creates a client.
std::shared_ptr<client> add_client();
std::shared_ptr<client> add_client(std::shared_ptr<struct sock>);

using clist = std::list<std::shared_ptr<client>>;
const clist &clients();

}      // namespace ircd
#endif // __cplusplus

/*
inline bool
is(const client &client, const mode::mask &mask)
{
	return mode::is(client.mode, mask);
}

inline void
set(client &client, const mode::mask &mask)
{
	return mode::set(client.mode, mask);
}

inline void
clear(client &client, const mode::mask &mask)
{
	return mode::clear(client.mode, mask);
}

inline void
set_got_id(client &client)
{
	client.flags |= flags::GOTID;
}

inline bool
is_got_id(const client &client)
{
	return (client.flags & flags::GOTID) != 0;
}


inline bool
is_exempt_kline(const client &client)
{
	return client.flags & flags::EXEMPTKLINE;
}

inline void
set_exempt_kline(client &client)
{
	client.flags |= flags::EXEMPTKLINE;
}

inline bool
is_exempt_flood(const client &client)
{
	return client.flags & flags::EXEMPTFLOOD;
}

inline void
set_exempt_flood(client &client)
{
	client.flags |= flags::EXEMPTFLOOD;
}

inline bool
is_exempt_spambot(const client &client)
{
	return client.flags & flags::EXEMPTSPAMBOT;
}

inline void
set_exempt_spambot(client &client)
{
	client.flags |= flags::EXEMPTSPAMBOT;
}

inline bool
is_exempt_shide(const client &client)
{
	return client.flags & flags::EXEMPTSHIDE;
}

inline void
set_exempt_shide(client &client)
{
	client.flags |= flags::EXEMPTSHIDE;
}

inline bool
is_exempt_jupe(const client &client)
{
	return client.flags & flags::EXEMPTJUPE;
}

inline void
set_exempt_jupe(client &client)
{
	client.flags |= flags::EXEMPTJUPE;
}

inline bool
is_exempt_resv(const client &client)
{
	return client.flags & flags::EXEMPTRESV;
}

inline void
set_exempt_resv(client &client)
{
	client.flags |= flags::EXEMPTRESV;
}

inline bool
is_ip_spoof(const client &client)
{
	return client.flags & flags::IP_SPOOFING;
}

inline void
set_ip_spoof(client &client)
{
	client.flags |= flags::IP_SPOOFING;
}

inline bool
is_extend_chans(const client &client)
{
	return client.flags & flags::EXTENDCHANS;
}

inline void
set_extend_chans(client &client)
{
	client.flags |= flags::EXTENDCHANS;
}

inline bool
is_client(const client &client)
{
	return client.status == status::CLIENT;
}

inline bool
is_registered_user(const client &client)
{
	return client.status == status::CLIENT;
}

inline bool
is_registered(const client &client)
{
	return client.status > status::UNKNOWN && client.status != status::REJECT;
}

inline bool
is_connecting(const client &client)
{
	return client.status == status::CONNECTING;
}

inline bool
is_handshake(const client &client)
{
	return client.status == status::HANDSHAKE;
}

inline bool
is_me(const client &client)
{
	return client.status == status::ME;
}

inline bool
is_unknown(const client &client)
{
	return client.status == status::UNKNOWN;
}

inline bool
is_server(const client &client)
{
	return client.status == status::SERVER;
}

inline bool
is_reject(const client &client)
{
	return client.status == status::REJECT;
}

inline bool
is_any_server(const client &client)
{
	return is_server(client) || is_handshake(client) || is_connecting(client);
}

inline void
set_reject(client &client)
{
	client.status = status::REJECT;
	//client.handler = UNREGISTERED_HANDLER;
}

inline void
set_connecting(client &client)
{
	client.status = status::CONNECTING;
	//client.handler = UNREGISTERED_HANDLER;
}

inline void
set_handshake(client &client)
{
	client.status = status::HANDSHAKE;
	//client.handler = UNREGISTERED_HANDLER;
}

inline void
set_me(client &client)
{
	client.status = status::ME;
	//client.handler = UNREGISTERED_HANDLER;
}

inline void
set_unknown(client &client)
{
	client.status = status::UNKNOWN;
	//client.handler = UNREGISTERED_HANDLER;
}

inline void
set_server(client &client)
{
	client.status = status::SERVER;
	//client.handler = SERVER_HANDLER;
}

bool is_oper(const client &);

inline void
set_client(client &client)
{
	client.status = status::CLIENT;
	//client.handler = is_oper(client)? OPER_HANDLER : CLIENT_HANDLER;
}

inline void
set_remote_client(client &client)
{
	client.status = status::CLIENT;
	//client.handler = RCLIENT_HANDLER;
}

inline bool
parse_as_client(const client &client)
{
	return bool(client.status & (status::UNKNOWN | status::CLIENT));
}

inline bool
parse_as_server(const client &client)
{
	return bool(client.status & (status::CONNECTING | status::HANDSHAKE | status::SERVER));
}


#define TS_CURRENT	6
#define TS_MIN          6

#define TS_DOESTS       0x10000000
#define DoesTS(x)       ((x)->tsinfo & TS_DOESTS)


inline bool
has_id(const client &client)
{
	return client.id[0] != '\0';
}

inline bool
has_id(const client *const &client)
{
	return has_id(*client);
}

inline const char *
use_id(const client &client)
{
	return has_id(client)? client.id : client.name;
}

inline const char *
use_id(const client *const &client)
{
	return use_id(*client);
}

bool is_server(const client &client);

inline char *get_id(const client *const &source, const client *const &target)
{
	return const_cast<char *>(is_server(*target->from) && has_id(target->from) ? use_id(source) : (source)->name);
}

inline auto
get_id(const client &source, const client &target)
{
	return get_id(&source, &target);
}

inline auto
id(const client &source, const client &target)
{
	return is_server(*target.from) && has_id(*target.from)? use_id(source) : source.name;
}



#define LFLAGS_SSL		0x00000001
#define LFLAGS_FLUSH		0x00000002


inline bool
is_person(const client &client)
{
	return is_client(client) && client.user;
}

inline bool
my_connect(const client &client)
{
	return client.flags & flags::MYCONNECT;
}

inline void
set_my_connect(client &client)
{
	client.flags |= flags::MYCONNECT;
}

inline void
clear_my_connect(client &client)
{
	client.flags &= ~flags::MYCONNECT;
}

inline bool
my(const client &client)
{
	return my_connect(client) && is_client(client);
}

inline bool
my_oper(const client &client)
{
	return my_connect(client) && is_oper(client);
}

inline bool
is_oper(const client &client)
{
	return is(client, mode::OPER);
}

inline void
set_oper(client &client)
{
	set(client, mode::OPER);
//	if (my(client))
//		client.handler = OPER_HANDLER;
}

inline void
clear_oper(client &client)
{
	clear(client, mode::OPER);
	clear(client, mode::ADMIN);
//	if (my(client) && !is_server(client))
//		client.handler = CLIENT_HANDLER;
}

inline void
set_mark(client &client)
{
	client.flags |= flags::MARK;
}

inline void
clear_mark(client &client)
{
	client.flags &= ~flags::MARK;
}

inline bool
is_marked(const client &client)
{
	return client.flags & flags::MARK;
}

inline void
set_hidden(client &client)
{
	client.flags |= flags::HIDDEN;
}

inline void
clear_hidden(client &client)
{
	client.flags &= ~flags::HIDDEN;
}

inline bool
is_hidden(const client &client)
{
	return client.flags & flags::HIDDEN;
}

inline void
clear_eob(client &client)
{
	client.flags &= ~flags::EOB;
}

inline void
set_eob(client &client)
{
	client.flags |= flags::EOB;
}

inline bool
has_sent_eob(const client &client)
{
	return client.flags & flags::EOB;
}

inline void
set_dead(client &client)
{
	client.flags |= flags::DEAD;
}

inline bool
is_dead(const client &client)
{
	return client.flags &  flags::DEAD;
}

inline void
set_closing(client &client)
{
	client.flags |= flags::CLOSING;
}

inline bool
is_closing(const client &client)
{
	return client.flags & flags::CLOSING;
}

inline void
set_io_error(client &client)
{
	client.flags |= flags::IOERROR;
}

inline bool
is_io_error(const client &client)
{
	return client.flags & flags::IOERROR;
}

inline bool
is_any_dead(const client &client)
{
	return is_io_error(client) || is_dead(client) || is_closing(client);
}

inline void
set_tg_change(client &client)
{
	client.flags |= flags::TGCHANGE;
}

inline void
clear_tg_change(client &client)
{
	client.flags &= ~flags::TGCHANGE;
}

inline bool
is_tg_change(const client &client)
{
	return client.flags & flags::TGCHANGE;
}

inline void
set_dyn_spoof(client &client)
{
	client.flags |= flags::DYNSPOOF;
}

inline void
clear_dyn_spoof(client &client)
{
	client.flags &= ~flags::DYNSPOOF;
}

inline bool
is_dyn_spoof(const client &client)
{
	return client.flags & flags::DYNSPOOF;
}

inline void
set_tg_excessive(client &client)
{
	client.flags|= flags::TGEXCESSIVE;
}

inline void
clear_tg_excessive(client &client)
{
	client.flags &= ~flags::TGEXCESSIVE;
}

inline bool
is_tg_excessive(const client &client)
{
	return client.flags & flags::TGEXCESSIVE;
}

inline void
set_flood_done(client &client)
{
	client.flags |= flags::FLOODDONE;
}

inline bool
is_flood_done(const client &client)
{
	return client.flags & flags::FLOODDONE;
}

inline bool
operator==(const client &a, const client &b)
{
	return &a == &b;
}

inline bool
operator!=(const client &a, const client &b)
{
	return !(a == b);
}

*/
