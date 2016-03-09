/*
 *  ircd-ratbox: A slightly useful ircd.
 *  m_unreject.c: Removes an ip from the reject cache
 *
 *  Copyright (C) 2004 Aaron Sethman <androsyn@ratbox.org>
 *  Copyright (C) 2004-2005 ircd-ratbox development team
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

#include "stdinc.h"
#include "client.h"
#include "s_conf.h"
#include "hostmask.h"
#include "reject.h"
#include "msg.h"
#include "modules.h"
#include "send.h"

static const char unreject_desc[] =
	"Provides the UNREJECT command to remove an IP from the reject cache";

static void mo_unreject(struct MsgBuf *, struct Client *, struct Client *, int, const char **);

struct Message unreject_msgtab = {
	"UNREJECT", 0, 0, 0, 0,
	{mg_unreg, mg_not_oper, mg_ignore, mg_ignore, mg_ignore, {mo_unreject, 2}}
};

mapi_clist_av1 unreject_clist[] = { &unreject_msgtab, NULL };

DECLARE_MODULE_AV2(unreject, NULL, NULL, unreject_clist, NULL, NULL, NULL, NULL, unreject_desc);

/*
 * mo_unreject
 */
static void
mo_unreject(struct MsgBuf *msgbuf_p, struct Client *client_p, struct Client *source_p, int parc, const char *parv[])
{
	if(ConfigFileEntry.reject_after_count == 0 || ConfigFileEntry.reject_ban_time == 0 ||
	   ConfigFileEntry.reject_duration == 0)
	{
		sendto_one_notice(source_p, ":Reject cache is disabled");
		return;
	}

	if(!parse_netmask(parv[1], NULL, NULL))
	{
		sendto_one_notice(source_p, ":Unable to parse netmask %s", parv[1]);
		return;
	}

	if(remove_reject_ip(parv[1]))
		sendto_one_notice(source_p, ":Removed reject for %s", parv[1]);
	else
		sendto_one_notice(source_p, ":Unable to remove reject for %s", parv[1]);
}
