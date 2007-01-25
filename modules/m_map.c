/*
 *  ircd-ratbox: A slightly useful ircd.
 *  m_map.c: Sends an Undernet compatible map to a user.
 *
 *  Copyright (C) 2002 by the past and present ircd coders, and others.
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
 *  $Id: m_map.c 254 2005-09-21 23:35:12Z nenolod $
 */

#include "stdinc.h"
#include "client.h"
#include "modules.h"
#include "numeric.h"
#include "send.h"
#include "s_conf.h"
#include "sprintf_irc.h"

#define USER_COL       50	/* display | Users: %d at col 50 */

static int m_map(struct Client *client_p, struct Client *source_p, int parc, const char *parv[]);
static int mo_map(struct Client *client_p, struct Client *source_p, int parc, const char *parv[]);

struct Message map_msgtab = {
	"MAP", 0, 0, 0, MFLG_SLOW,
	{mg_unreg, {m_map, 0}, mg_ignore, mg_ignore, mg_ignore, {mo_map, 0}}
};

mapi_clist_av1 map_clist[] = { &map_msgtab, NULL };
DECLARE_MODULE_AV1(map, NULL, NULL, map_clist, NULL, NULL, "$Revision: 254 $");

static void dump_map(struct Client *client_p, struct Client *root, char *pbuf);

static char buf[BUFSIZE];

/* m_map
**	parv[0] = sender prefix
*/
static int
m_map(struct Client *client_p, struct Client *source_p, int parc, const char *parv[])
{
	if((!IsExemptShide(source_p) && ConfigServerHide.flatten_links) ||
	   ConfigFileEntry.map_oper_only)
	{
		m_not_oper(client_p, source_p, parc, parv);
		return 0;
	}

	dump_map(client_p, &me, buf);
	sendto_one(client_p, form_str(RPL_MAPEND), me.name, client_p->name);
	return 0;
}

/*
** mo_map
**      parv[0] = sender prefix
*/
static int
mo_map(struct Client *client_p, struct Client *source_p, int parc, const char *parv[])
{
	dump_map(client_p, &me, buf);
	sendto_one(client_p, form_str(RPL_MAPEND), me.name, client_p->name);

	return 0;
}

/*
** dump_map
**   dumps server map, called recursively.
*/
static void
dump_map(struct Client *client_p, struct Client *root_p, char *pbuf)
{
	int cnt = 0, i = 0, len;
	struct Client *server_p;
	dlink_node *ptr;
	*pbuf = '\0';

	strlcat(pbuf, root_p->name, BUFSIZE);
	if (has_id(root_p))
	{
		strlcat(pbuf, "[", BUFSIZE);
		strlcat(pbuf, root_p->id, BUFSIZE);
		strlcat(pbuf, "]", BUFSIZE);
	}
	len = strlen(buf);
	buf[len] = ' ';

	if(len < USER_COL)
	{
		for (i = len + 1; i < USER_COL; i++)
		{
			buf[i] = '-';
		}
	}

	ircsnprintf(buf + USER_COL, BUFSIZE - USER_COL,
		 " | Users: %5lu (%4.1f%%)", dlink_list_length(&root_p->serv->users),
		 100 * (float) dlink_list_length(&root_p->serv->users) / (float) Count.total);

	sendto_one(client_p, form_str(RPL_MAP), me.name, client_p->name, buf);

	if(root_p->serv->servers.head != NULL)
	{
		cnt += dlink_list_length(&root_p->serv->servers);

		if(cnt)
		{
			if(pbuf > buf + 3)
			{
				pbuf[-2] = ' ';
				if(pbuf[-3] == '`')
					pbuf[-3] = ' ';
			}
		}
	}
	i = 1;
	DLINK_FOREACH(ptr, root_p->serv->servers.head)
	{
		server_p = ptr->data;
		*pbuf = ' ';
		if(i < cnt)
			*(pbuf + 1) = '|';
		else
			*(pbuf + 1) = '`';

		*(pbuf + 2) = '-';
		*(pbuf + 3) = ' ';
		dump_map(client_p, server_p, pbuf + 4);

		i++;
	}
}
