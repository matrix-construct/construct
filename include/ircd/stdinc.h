/*
 *  ircd-ratbox: A slightly useful ircd.
 *  stdinc.h: Pull in all of the necessary system headers
 *
 *  Copyright (C) 2002 Aaron Sethman <androsyn@ratbox.org>
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
 *
 */

#if defined(PIC) && defined(PCH)
#include <rb/rb.pic.h>
#else
#include <rb/rb.h>
#endif

namespace ircd
{
	using std::begin;
	using std::end;
	using std::get;
	using ostream = std::ostream;
	using namespace std::string_literals;
}

// Temp fwd decl scaffold
namespace ircd
{
	namespace client
	{
		struct client;
		struct LocalUser;
		struct PreClient;
		struct ListClient;
	}

	namespace chan
	{
		struct chan;
		struct membership;
	}

	struct ConfItem;
	struct DNSReply;
	struct Listener;
	struct Blacklist;
	struct PrivilegeSet;
	struct _ssl_ctl;
	struct ev_ctl;
	struct ws_ctl;
	struct server_conf;
}

#include "util.h"
#include "defaults.h"
#include "exception.h"
#include "numeric.h"
#include "messages.h"
#include "rfc1459.h"
#include "err.h"
#include "fs.h"
#include "s_assert.h"
#include "match.h"
#include "mode_table.h"
#include "mode_lease.h"
#include "snomask.h"

#include "cache.h"
#include "whowas.h"
#include "tgchange.h"
#include "msgbuf.h"
#include "msg.h"
#include "client.h"

#include "mask.h"
#include "chmode.h"
#include "channel.h"

#include "authproc.h"
#include "bandbi.h"
#include "capability.h"
#include "certfp.h"
#include "class.h"
#include "dns.h"
#include "hash.h"
#include "hook.h"
#include "ircd_getopt.h"
#include "listener.h"
#include "logger.h"
#include "info.h"
#include "modules.h"
#include "monitor.h"
#include "newconf.h"
#include "operhash.h"
#include "packet.h"
#include "parse.h"
#include "privilege.h"
#include "ratelimit.h"
#include "reject.h"
#include "s_conf.h"
#include "send.h"
#include "s_newconf.h"
#include "s_serv.h"
#include "sslproc.h"
#include "s_stats.h"
#include "substitution.h"
#include "supported.h"
#include "s_user.h"
#include "wsproc.h"
#include "stringops.h"
