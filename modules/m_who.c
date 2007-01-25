/*
 *  ircd-ratbox: A slightly useful ircd.
 *  m_who.c: Shows who is on a channel.
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
 *  $Id: m_who.c 1853 2006-08-24 18:30:52Z jilles $
 */
#include "stdinc.h"
#include "tools.h"
#include "common.h"
#include "client.h"
#include "channel.h"
#include "hash.h"
#include "ircd.h"
#include "numeric.h"
#include "s_serv.h"
#include "send.h"
#include "irc_string.h"
#include "sprintf_irc.h"
#include "s_conf.h"
#include "s_log.h"
#include "msg.h"
#include "parse.h"
#include "modules.h"
#include "packet.h"
#include "s_newconf.h"

static int m_who(struct Client *, struct Client *, int, const char **);

struct Message who_msgtab = {
	"WHO", 0, 0, 0, MFLG_SLOW,
	{mg_unreg, {m_who, 2}, mg_ignore, mg_ignore, mg_ignore, {m_who, 2}}
};

mapi_clist_av1 who_clist[] = { &who_msgtab, NULL };
DECLARE_MODULE_AV1(who, NULL, NULL, who_clist, NULL, NULL, "$Revision: 1853 $");

static void do_who_on_channel(struct Client *source_p, struct Channel *chptr,
			      int server_oper, int member);

static void who_global(struct Client *source_p, const char *mask, int server_oper, int operspy);

static void do_who(struct Client *source_p,
		   struct Client *target_p, const char *chname, const char *op_flags);


/*
** m_who
**      parv[0] = sender prefix
**      parv[1] = nickname mask list
**      parv[2] = additional selection flag, only 'o' for now.
*/
static int
m_who(struct Client *client_p, struct Client *source_p, int parc, const char *parv[])
{
	static time_t last_used = 0;
	struct Client *target_p;
	struct membership *msptr;
	char *mask;
	dlink_node *lp;
	struct Channel *chptr = NULL;
	int server_oper = parc > 2 ? (*parv[2] == 'o') : 0;	/* Show OPERS only */
	int member;
	int operspy = 0;

	mask = LOCAL_COPY(parv[1]);

	collapse(mask);

	/* '/who *' */
	if((*(mask + 1) == '\0') && (*mask == '*'))
	{
		if(source_p->user == NULL)
			return 0;

		if((lp = source_p->user->channel.head) != NULL)
		{
			msptr = lp->data;
			do_who_on_channel(source_p, msptr->chptr, server_oper, YES);
		}

		sendto_one(source_p, form_str(RPL_ENDOFWHO),
			   me.name, source_p->name, "*");
		return 0;
	}

	if(IsOperSpy(source_p) && *mask == '!')
	{
		mask++;
		operspy = 1;

		if(EmptyString(mask))
		{
			sendto_one(source_p, form_str(RPL_ENDOFWHO),
					me.name, source_p->name, parv[1]);
			return 0;
		}
	}

	/* '/who #some_channel' */
	if(IsChannelName(mask))
	{
		/* List all users on a given channel */
		chptr = find_channel(mask);
		if(chptr != NULL)
		{
			if(operspy)
				report_operspy(source_p, "WHO", chptr->chname);

			if(IsMember(source_p, chptr) || operspy)
				do_who_on_channel(source_p, chptr, server_oper, YES);
			else if(!SecretChannel(chptr))
				do_who_on_channel(source_p, chptr, server_oper, NO);
		}
		sendto_one(source_p, form_str(RPL_ENDOFWHO),
			   me.name, source_p->name, mask);
		return 0;
	}

	/* '/who nick' */

	if(((target_p = find_named_person(mask)) != NULL) &&
	   (!server_oper || IsOper(target_p)))
	{
		int isinvis = 0;

		isinvis = IsInvisible(target_p);
		DLINK_FOREACH(lp, target_p->user->channel.head)
		{
			msptr = lp->data;
			chptr = msptr->chptr;

			member = IsMember(source_p, chptr);

			if(isinvis && !member)
				continue;

			if(member || (!isinvis && PubChannel(chptr)))
				break;
		}

		/* if we stopped midlist, lp->data is the membership for
		 * target_p of chptr
		 */
		if(lp != NULL)
			do_who(source_p, target_p, chptr->chname,
			       find_channel_status(lp->data, IsCapable(source_p, CLICAP_MULTI_PREFIX)));
		else
			do_who(source_p, target_p, NULL, "");

		sendto_one(source_p, form_str(RPL_ENDOFWHO), 
			   me.name, source_p->name, mask);
		return 0;
	}

	if(!IsFloodDone(source_p))
		flood_endgrace(source_p);

	/* it has to be a global who at this point, limit it */
	if(!IsOper(source_p))
	{
		if((last_used + ConfigFileEntry.pace_wait) > CurrentTime)
		{
			sendto_one(source_p, form_str(RPL_LOAD2HI),
					me.name, source_p->name, "WHO");
			sendto_one(source_p, form_str(RPL_ENDOFWHO),
				   me.name, source_p->name, "*");
			return 0;
		}
		else
			last_used = CurrentTime;
	}

	/* Note: operspy_dont_care_user_info does not apply to
	 * who on channels */
	if(IsOperSpy(source_p) && ConfigFileEntry.operspy_dont_care_user_info)
		operspy = 1;

	/* '/who 0' for a global list.  this forces clients to actually
	 * request a full list.  I presume its because of too many typos
	 * with "/who" ;) --fl
	 */
	if((*(mask + 1) == '\0') && (*mask == '0'))
		who_global(source_p, NULL, server_oper, 0);
	else
		who_global(source_p, mask, server_oper, operspy);

	sendto_one(source_p, form_str(RPL_ENDOFWHO),
		   me.name, source_p->name, mask);

	return 0;
}

/* who_common_channel
 * inputs	- pointer to client requesting who
 * 		- pointer to channel member chain.
 *		- char * mask to match
 *		- int if oper on a server or not
 *		- pointer to int maxmatches
 * output	- NONE
 * side effects - lists matching invisible clients on specified channel,
 * 		  marks matched clients.
 */
static void
who_common_channel(struct Client *source_p, struct Channel *chptr,
		   const char *mask, int server_oper, int *maxmatches)
{
	struct membership *msptr;
	struct Client *target_p;
	dlink_node *ptr;

	DLINK_FOREACH(ptr, chptr->members.head)
	{
		msptr = ptr->data;
		target_p = msptr->client_p;

		if(!IsInvisible(target_p) || IsMarked(target_p))
			continue;

		if(server_oper && !IsOper(target_p))
			continue;

		SetMark(target_p);

		if(*maxmatches > 0)
		{
			if((mask == NULL) ||
					match(mask, target_p->name) || match(mask, target_p->username) ||
					match(mask, target_p->host) || match(mask, target_p->user->server) ||
					(IsOper(source_p) && match(mask, target_p->orighost)) ||
					match(mask, target_p->info))
			{
				do_who(source_p, target_p, NULL, "");
				--(*maxmatches);
			}
		}
	}
}

/*
 * who_global
 *
 * inputs	- pointer to client requesting who
 *		- char * mask to match
 *		- int if oper on a server or not
 * output	- NONE
 * side effects - do a global scan of all clients looking for match
 *		  this is slightly expensive on EFnet ...
 *		  marks assumed cleared for all clients initially
 *		  and will be left cleared on return
 */
static void
who_global(struct Client *source_p, const char *mask, int server_oper, int operspy)
{
	struct membership *msptr;
	struct Client *target_p;
	dlink_node *lp, *ptr;
	int maxmatches = 500;

	/* first, list all matching INvisible clients on common channels
	 * if this is not an operspy who
	 */
	if(!operspy)
	{
		DLINK_FOREACH(lp, source_p->user->channel.head)
		{
			msptr = lp->data;
			who_common_channel(source_p, msptr->chptr, mask, server_oper, &maxmatches);
		}
	}
	else if (!ConfigFileEntry.operspy_dont_care_user_info)
		report_operspy(source_p, "WHO", mask);

	/* second, list all matching visible clients and clear all marks
	 * on invisible clients
	 * if this is an operspy who, list all matching clients, no need
	 * to clear marks
	 */
	DLINK_FOREACH(ptr, global_client_list.head)
	{
		target_p = ptr->data;
		if(!IsPerson(target_p))
			continue;

		if(IsInvisible(target_p) && !operspy)
		{
			ClearMark(target_p);
			continue;
		}

		if(server_oper && !IsOper(target_p))
			continue;

		if(maxmatches > 0)
		{
			if(!mask ||
					match(mask, target_p->name) || match(mask, target_p->username) ||
					match(mask, target_p->host) || match(mask, target_p->user->server) ||
					(IsOper(source_p) && match(mask, target_p->orighost)) ||
					match(mask, target_p->info))
			{
				do_who(source_p, target_p, NULL, "");
				--maxmatches;
			}
		}
	}

	if (maxmatches <= 0)
		sendto_one(source_p,
			form_str(ERR_TOOMANYMATCHES),
			me.name, source_p->name, "WHO");
}

/*
 * do_who_on_channel
 *
 * inputs	- pointer to client requesting who
 *		- pointer to channel to do who on
 *		- The "real name" of this channel
 *		- int if source_p is a server oper or not
 *		- int if client is member or not
 * output	- NONE
 * side effects - do a who on given channel
 */
static void
do_who_on_channel(struct Client *source_p, struct Channel *chptr,
		  int server_oper, int member)
{
	struct Client *target_p;
	struct membership *msptr;
	dlink_node *ptr;
	int combine = IsCapable(source_p, CLICAP_MULTI_PREFIX);

	DLINK_FOREACH(ptr, chptr->members.head)
	{
		msptr = ptr->data;
		target_p = msptr->client_p;

		if(server_oper && !IsOper(target_p))
			continue;

		if(member || !IsInvisible(target_p))
			do_who(source_p, target_p, chptr->chname,
			       find_channel_status(msptr, combine));
	}
}

/*
 * do_who
 *
 * inputs	- pointer to client requesting who
 *		- pointer to client to do who on
 *		- The reported name
 *		- channel flags
 * output	- NONE
 * side effects - do a who on given person
 */

static void
do_who(struct Client *source_p, struct Client *target_p, const char *chname, const char *op_flags)
{
	char status[5];

	ircsprintf(status, "%c%s%s",
		   target_p->user->away ? 'G' : 'H', IsOper(target_p) ? "*" : "", op_flags);

	sendto_one(source_p, form_str(RPL_WHOREPLY), me.name, source_p->name,
		   (chname) ? (chname) : "*",
		   target_p->username,
		   target_p->host, target_p->user->server, target_p->name,
		   status, 
		   ConfigServerHide.flatten_links ? 0 : target_p->hopcount, 
		   target_p->info);
}
