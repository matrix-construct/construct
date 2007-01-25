/*
 *  ircd-ratbox: A slightly useful ircd.
 *  m_help.c: Provides help information to a user/operator.
 *
 *  Copyright (C) 1990 Jarkko Oikarinen and University of Oulu, Co Center
 *  Copyright (C) 1996-2002 Hybrid Development Team
 *  Copyright (C) 2002-2005 ircd-ratbox development team
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
 *  $Id: m_help.c 254 2005-09-21 23:35:12Z nenolod $
 */

#include "stdinc.h"
#include "client.h"
#include "ircd.h"
#include "msg.h"
#include "numeric.h"
#include "send.h"
#include "s_conf.h"
#include "s_log.h"
#include "parse.h"
#include "modules.h"
#include "hash.h"
#include "cache.h"

static int m_help(struct Client *, struct Client *, int, const char **);
static int mo_help(struct Client *, struct Client *, int, const char **);
static int mo_uhelp(struct Client *, struct Client *, int, const char **);
static void dohelp(struct Client *, int, const char *);

struct Message help_msgtab = {
	"HELP", 0, 0, 0, MFLG_SLOW,
	{mg_unreg, {m_help, 0}, mg_ignore, mg_ignore, mg_ignore, {mo_help, 0}}
};
struct Message uhelp_msgtab = {
	"UHELP", 0, 0, 0, MFLG_SLOW,
	{mg_unreg, {m_help, 0}, mg_ignore, mg_ignore, mg_ignore, {mo_uhelp, 0}}
};

mapi_clist_av1 help_clist[] = { &help_msgtab, &uhelp_msgtab, NULL };
DECLARE_MODULE_AV1(help, NULL, NULL, help_clist, NULL, NULL, "$Revision: 254 $");

/*
 * m_help - HELP message handler
 *      parv[0] = sender prefix
 */
static int
m_help(struct Client *client_p, struct Client *source_p, int parc, const char *parv[])
{
	static time_t last_used = 0;

	/* HELP is always local */
	if((last_used + ConfigFileEntry.pace_wait_simple) > CurrentTime)
	{
		/* safe enough to give this on a local connect only */
		sendto_one(source_p, form_str(RPL_LOAD2HI), 
			   me.name, source_p->name, "HELP");
		sendto_one(source_p, form_str(RPL_ENDOFHELP),
			   me.name, source_p->name,
			   (parc > 1 && !EmptyString(parv[1])) ? parv[1] : "index");
		return 0;
	}
	else
	{
		last_used = CurrentTime;
	}

	dohelp(source_p, HELP_USER, parc > 1 ? parv[1] : NULL);

	return 0;
}

/*
 * mo_help - HELP message handler
 *      parv[0] = sender prefix
 */
static int
mo_help(struct Client *client_p, struct Client *source_p, int parc, const char *parv[])
{
	dohelp(source_p, HELP_OPER, parc > 1 ? parv[1] : NULL);
	return 0;
}

/*
 * mo_uhelp - HELP message handler
 * This is used so that opers can view the user help file without deopering
 *      parv[0] = sender prefix
 */
static int
mo_uhelp(struct Client *client_p, struct Client *source_p, int parc, const char *parv[])
{
	dohelp(source_p, HELP_USER, parc > 1 ? parv[1] : NULL);
	return 0;
}

static void
dohelp(struct Client *source_p, int flags, const char *topic)
{
	static const char ntopic[] = "index";
	struct cachefile *hptr;
	struct cacheline *lineptr;
	dlink_node *ptr;
	dlink_node *fptr;

	if(EmptyString(topic))
		topic = ntopic;

	hptr = hash_find_help(topic, flags);

	if(hptr == NULL)
	{
		sendto_one(source_p, form_str(ERR_HELPNOTFOUND),
			   me.name, source_p->name, topic);
		return;
	}

	fptr = hptr->contents.head;
	lineptr = fptr->data;

	/* first line cant be empty */
	sendto_one(source_p, form_str(RPL_HELPSTART),
		   me.name, source_p->name, topic, lineptr->data);

	DLINK_FOREACH(ptr, fptr->next)
	{
		lineptr = ptr->data;

		sendto_one(source_p, form_str(RPL_HELPTXT),
			   me.name, source_p->name, topic, lineptr->data);
	}

	sendto_one(source_p, form_str(RPL_ENDOFHELP),
		   me.name, source_p->name, topic);
	return;
}
