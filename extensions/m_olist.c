/*
 *  ircd-ratbox: A slightly useful ircd.
 *  m_olist.c: List channels.  olist is an oper only command
 *             that shows channels regardless of modes.  This
 *             is kinda evil, and might be morally wrong, but
 *             somebody will likely need it.
 *
 *  Copyright (C) 2002 by the past and present ircd coders, and others.
 *  Copyright (C) 2004 ircd-ratbox Development Team
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
 *  $Id: m_olist.c 6 2005-09-10 01:02:21Z nenolod $
 */

#include "stdinc.h"
#include "channel.h"
#include "client.h"
#include "ircd.h"
#include "numeric.h"
#include "logger.h"
#include "s_serv.h"
#include "send.h"
#include "whowas.h"
#include "match.h"
#include "hash.h"
#include "msg.h"
#include "parse.h"
#include "modules.h"
#include "s_newconf.h"

static int mo_olist(struct Client *, struct Client *, int parc, const char *parv[]);

#ifndef STATIC_MODULES

struct Message olist_msgtab = {
	"OLIST", 0, 0, 0, MFLG_SLOW,
	{mg_unreg, mg_not_oper, mg_ignore, mg_ignore, mg_ignore, {mo_olist, 1}}
};

mapi_clist_av1 olist_clist[] = { &olist_msgtab, NULL };

DECLARE_MODULE_AV1(okick, NULL, NULL, olist_clist, NULL, NULL, "$Revision: 6 $");

#endif

static void list_all_channels(struct Client *source_p);
static void list_named_channel(struct Client *source_p, const char *name);

/*
** mo_olist
**      parv[1] = channel
*/
static int
mo_olist(struct Client *client_p, struct Client *source_p, int parc, const char *parv[])
{
	if(!IsOperSpy(source_p))
	{
		sendto_one(source_p, form_str(ERR_NOPRIVS),
				me.name, source_p->name, "oper_spy");
		sendto_one(source_p, form_str(RPL_LISTEND),
				me.name, source_p->name);
		return 0;
	}

	/* If no arg, do all channels *whee*, else just one channel */
	if(parc < 2 || EmptyString(parv[1]))
		list_all_channels(source_p);
	else
		list_named_channel(source_p, parv[1]);

	sendto_one(source_p, form_str(RPL_LISTEND), me.name, source_p->name);
	return 0;
}


/*
 * list_all_channels
 * inputs	- pointer to client requesting list
 * output	- 0/1
 * side effects	- list all channels to source_p
 */
static void
list_all_channels(struct Client *source_p)
{
	struct Channel *chptr;
	rb_dlink_node *ptr;

	report_operspy(source_p, "LIST", NULL);
	sendto_one(source_p, form_str(RPL_LISTSTART), me.name, source_p->name);

	RB_DLINK_FOREACH(ptr, global_channel_list.head)
	{
		chptr = ptr->data;

		sendto_one(source_p, form_str(RPL_LIST),
				me.name, source_p->name, chptr->chname,
				rb_dlink_list_length(&chptr->members),
				chptr->topic == NULL ? "" : chptr->topic);
	}

	return;
}

/*
 * list_named_channel
 * inputs       - pointer to client requesting list
 * output       - 0/1
 * side effects	- list all channels to source_p
 */
static void
list_named_channel(struct Client *source_p, const char *name)
{
	struct Channel *chptr;
	char *p;
	char *n = LOCAL_COPY(name);

	if((p = strchr(n, ',')))
		*p = '\0';

	/* Put operspy notice before any output, but only if channel exists */
	chptr = EmptyString(n) ? NULL : find_channel(n);
	if(chptr != NULL)
		report_operspy(source_p, "LIST", chptr->chname);

	sendto_one(source_p, form_str(RPL_LISTSTART), me.name, source_p->name);

	if(EmptyString(n))
		return;

	if(chptr == NULL)
		sendto_one_numeric(source_p, ERR_NOSUCHCHANNEL,
				form_str(ERR_NOSUCHCHANNEL), n);
	else
		sendto_one(source_p, form_str(RPL_LIST), me.name, source_p->name,
			chptr->chname, rb_dlink_list_length(&chptr->members),
			chptr->topic ? chptr->topic : "");
}
