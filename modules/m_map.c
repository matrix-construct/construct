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
 *  $Id: m_map.c 3368 2007-04-03 10:11:06Z nenolod $
 */

#include "stdinc.h"
#include "client.h"
#include "modules.h"
#include "numeric.h"
#include "send.h"
#include "s_conf.h"
#include "scache.h"

#define USER_COL       50	/* display | Users: %d at col 50 */

static int m_map(struct Client *client_p, struct Client *source_p, int parc, const char *parv[]);
static int mo_map(struct Client *client_p, struct Client *source_p, int parc, const char *parv[]);

struct Message map_msgtab = {
	"MAP", 0, 0, 0, MFLG_SLOW,
	{mg_unreg, {m_map, 0}, mg_ignore, mg_ignore, mg_ignore, {mo_map, 0}}
};

mapi_clist_av1 map_clist[] = { &map_msgtab, NULL };
DECLARE_MODULE_AV1(map, NULL, NULL, map_clist, NULL, NULL, "$Revision: 3368 $");

static void dump_map(struct Client *client_p, struct Client *root, char *pbuf);
static void flattened_map(struct Client *client_p);

static char buf[BUFSIZE];

/* m_map
*/
static int
m_map(struct Client *client_p, struct Client *source_p, int parc, const char *parv[])
{
	if((!IsExemptShide(source_p) && ConfigServerHide.flatten_links) ||
	   ConfigFileEntry.map_oper_only)
	{
		flattened_map(client_p);
		sendto_one_numeric(client_p, RPL_MAPEND, form_str(RPL_MAPEND));
		return 0;
	}

	dump_map(client_p, &me, buf);
	sendto_one_numeric(client_p, RPL_MAPEND, form_str(RPL_MAPEND));
	return 0;
}

/*
** mo_map
*/
static int
mo_map(struct Client *client_p, struct Client *source_p, int parc, const char *parv[])
{
	dump_map(client_p, &me, buf);
	scache_send_missing(client_p);
	sendto_one_numeric(client_p, RPL_MAPEND, form_str(RPL_MAPEND));

	return 0;
}

/*
** dump_map
**   dumps server map, called recursively.
*/
static void
dump_map(struct Client *client_p, struct Client *root_p, char *pbuf)
{
	int cnt = 0, i = 0, len, frac;
	struct Client *server_p;
	rb_dlink_node *ptr;
	*pbuf = '\0';

	rb_strlcat(pbuf, root_p->name, BUFSIZE);
	if (has_id(root_p))
	{
		rb_strlcat(pbuf, "[", BUFSIZE);
		rb_strlcat(pbuf, root_p->id, BUFSIZE);
		rb_strlcat(pbuf, "]", BUFSIZE);
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

	frac = (1000 * rb_dlink_list_length(&root_p->serv->users) + Count.total / 2) / Count.total;
	rb_snprintf(buf + USER_COL, BUFSIZE - USER_COL,
		 " | Users: %5lu (%2d.%1d%%)", rb_dlink_list_length(&root_p->serv->users),
		 frac / 10, frac % 10);

	sendto_one_numeric(client_p, RPL_MAP, form_str(RPL_MAP), buf);

	if(root_p->serv->servers.head != NULL)
	{
		cnt += rb_dlink_list_length(&root_p->serv->servers);

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
	RB_DLINK_FOREACH(ptr, root_p->serv->servers.head)
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

/*
 * flattened_map - display a version of map that doesn't give away routing
 *                 information to users when flattened links is enabled.
 */
static void
flattened_map(struct Client *client_p)
{
	char buf[BUFSIZE];
	rb_dlink_node *ptr;
	struct Client *target_p;
	int i, len;
	int cnt = 0;

	/* First display me as the root */
	rb_strlcpy(buf, me.name, BUFSIZE);
	len = strlen(buf);
	buf[len] = ' ';

	if(len < USER_COL)
	{
		for (i = len + 1; i < USER_COL; i++)
		{
			buf[i] = '-';
		}
	}

	snprintf(buf + USER_COL, BUFSIZE - USER_COL,
		" | Users: %5lu (%4.1f%%)", rb_dlink_list_length(&me.serv->users),
		100 * (float) rb_dlink_list_length(&me.serv->users) / (float) Count.total);

	sendto_one_numeric(client_p, RPL_MAP, form_str(RPL_MAP), buf);

	/* Next, we run through every other server and list them */
	RB_DLINK_FOREACH(ptr, global_serv_list.head)
	{
		target_p = ptr->data;

		cnt++;

		/* Skip ourselves, it's already counted */
		if(IsMe(target_p))
			continue;

		/* if we're hidden, go on to the next leaf */
		if(!ConfigServerHide.disable_hidden && IsHidden(target_p))
			continue;

		if (cnt == rb_dlink_list_length(&global_serv_list))
			rb_strlcpy(buf, " `- ", BUFSIZE);
		else
			rb_strlcpy(buf, " |- ", BUFSIZE);

		rb_strlcat(buf, target_p->name, BUFSIZE);
		len = strlen(buf);
		buf[len] = ' ';

		if(len < USER_COL)
		{
			for (i = len + 1; i < USER_COL; i++)
			{
				buf[i] = '-';
			}
		}

		snprintf(buf + USER_COL, BUFSIZE - USER_COL,
			" | Users: %5lu (%4.1f%%)", rb_dlink_list_length(&target_p->serv->users),
			100 * (float) rb_dlink_list_length(&target_p->serv->users) / (float) Count.total);

		sendto_one_numeric(client_p, RPL_MAP, form_str(RPL_MAP), buf);
	}
}
