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
 *  $Id: m_who.c 3350 2007-04-02 22:03:08Z jilles $
 */
#include "stdinc.h"
#include "common.h"
#include "client.h"
#include "channel.h"
#include "hash.h"
#include "ircd.h"
#include "numeric.h"
#include "s_serv.h"
#include "send.h"
#include "match.h"
#include "s_conf.h"
#include "logger.h"
#include "msg.h"
#include "parse.h"
#include "modules.h"
#include "packet.h"
#include "s_newconf.h"

#define FIELD_CHANNEL    0x0001
#define FIELD_HOP        0x0002
#define FIELD_FLAGS      0x0004
#define FIELD_HOST       0x0008
#define FIELD_IP         0x0010
#define FIELD_IDLE       0x0020
#define FIELD_NICK       0x0040
#define FIELD_INFO       0x0080
#define FIELD_SERVER     0x0100
#define FIELD_QUERYTYPE  0x0200 /* cookie for client */
#define FIELD_USER       0x0400
#define FIELD_ACCOUNT    0x0800
#define FIELD_OPLEVEL    0x1000 /* meaningless and stupid, but whatever */

struct who_format
{
	int fields;
	const char *querytype;
};

static int m_who(struct Client *, struct Client *, int, const char **);

struct Message who_msgtab = {
	"WHO", 0, 0, 0, MFLG_SLOW,
	{mg_unreg, {m_who, 2}, mg_ignore, mg_ignore, mg_ignore, {m_who, 2}}
};

mapi_clist_av1 who_clist[] = { &who_msgtab, NULL };
DECLARE_MODULE_AV1(who, NULL, NULL, who_clist, NULL, NULL, "$Revision: 3350 $");

static void do_who_on_channel(struct Client *source_p, struct Channel *chptr,
			      int server_oper, int member,
			      struct who_format *fmt);

static void who_global(struct Client *source_p, const char *mask, int server_oper, int operspy, struct who_format *fmt);

static void do_who(struct Client *source_p,
		   struct Client *target_p, struct membership *msptr,
		   struct who_format *fmt);


/*
** m_who
**      parv[1] = nickname mask list
**      parv[2] = additional selection flag and format options
*/
static int
m_who(struct Client *client_p, struct Client *source_p, int parc, const char *parv[])
{
	static time_t last_used = 0;
	struct Client *target_p;
	struct membership *msptr;
	char *mask;
	rb_dlink_node *lp;
	struct Channel *chptr = NULL;
	int server_oper = parc > 2 ? (*parv[2] == 'o') : 0;	/* Show OPERS only */
	int member;
	int operspy = 0;
	struct who_format fmt;
	const char *s;
	char maskcopy[512];

	fmt.fields = 0;
	fmt.querytype = NULL;
	if (parc > 2 && (s = strchr(parv[2], '%')) != NULL)
	{
		s++;
		for (; *s != '\0'; s++)
		{
			switch (*s)
			{
				case 'c': fmt.fields |= FIELD_CHANNEL; break;
				case 'd': fmt.fields |= FIELD_HOP; break;
				case 'f': fmt.fields |= FIELD_FLAGS; break;
				case 'h': fmt.fields |= FIELD_HOST; break;
				case 'i': fmt.fields |= FIELD_IP; break;
				case 'l': fmt.fields |= FIELD_IDLE; break;
				case 'n': fmt.fields |= FIELD_NICK; break;
				case 'r': fmt.fields |= FIELD_INFO; break;
				case 's': fmt.fields |= FIELD_SERVER; break;
				case 't': fmt.fields |= FIELD_QUERYTYPE; break;
				case 'u': fmt.fields |= FIELD_USER; break;
				case 'a': fmt.fields |= FIELD_ACCOUNT; break;
				case 'o': fmt.fields |= FIELD_OPLEVEL; break;
				case ',':
					  s++;
					  fmt.querytype = s;
					  s += strlen(s);
					  s--;
					  break;
			}
		}
		if (EmptyString(fmt.querytype) || strlen(fmt.querytype) > 3)
			fmt.querytype = "0";
	}

	rb_strlcpy(maskcopy, parv[1], sizeof maskcopy);
	mask = maskcopy;

	collapse(mask);

	/* '/who *' */
	if((*(mask + 1) == '\0') && (*mask == '*'))
	{
		if(source_p->user == NULL)
			return 0;

		if((lp = source_p->user->channel.head) != NULL)
		{
			msptr = lp->data;
			do_who_on_channel(source_p, msptr->chptr, server_oper, YES, &fmt);
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
		chptr = find_channel(parv[1] + operspy);
		if(chptr != NULL)
		{
			if(operspy)
				report_operspy(source_p, "WHO", chptr->chname);

			if(IsMember(source_p, chptr) || operspy)
				do_who_on_channel(source_p, chptr, server_oper, YES, &fmt);
			else if(!SecretChannel(chptr))
				do_who_on_channel(source_p, chptr, server_oper, NO, &fmt);
		}
		sendto_one(source_p, form_str(RPL_ENDOFWHO),
			   me.name, source_p->name, parv[1] + operspy);
		return 0;
	}

	/* '/who nick' */

	if(((target_p = find_named_person(mask)) != NULL) &&
	   (!server_oper || IsOper(target_p)))
	{
		int isinvis = 0;

		isinvis = IsInvisible(target_p);
		RB_DLINK_FOREACH(lp, target_p->user->channel.head)
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
			do_who(source_p, target_p, lp->data, &fmt);
		else
			do_who(source_p, target_p, NULL, &fmt);

		sendto_one(source_p, form_str(RPL_ENDOFWHO), 
			   me.name, source_p->name, mask);
		return 0;
	}

	if(!IsFloodDone(source_p))
		flood_endgrace(source_p);

	/* it has to be a global who at this point, limit it */
	if(!IsOper(source_p))
	{
		if((last_used + ConfigFileEntry.pace_wait) > rb_current_time())
		{
			sendto_one(source_p, form_str(RPL_LOAD2HI),
					me.name, source_p->name, "WHO");
			sendto_one(source_p, form_str(RPL_ENDOFWHO),
				   me.name, source_p->name, "*");
			return 0;
		}
		else
			last_used = rb_current_time();
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
		who_global(source_p, NULL, server_oper, 0, &fmt);
	else
		who_global(source_p, mask, server_oper, operspy, &fmt);

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
 *		- format options
 * output	- NONE
 * side effects - lists matching invisible clients on specified channel,
 * 		  marks matched clients.
 */
static void
who_common_channel(struct Client *source_p, struct Channel *chptr,
		   const char *mask, int server_oper, int *maxmatches,
		   struct who_format *fmt)
{
	struct membership *msptr;
	struct Client *target_p;
	rb_dlink_node *ptr;

	RB_DLINK_FOREACH(ptr, chptr->members.head)
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
					match(mask, target_p->host) || match(mask, target_p->servptr->name) ||
					(IsOper(source_p) && match(mask, target_p->orighost)) ||
					match(mask, target_p->info))
			{
				do_who(source_p, target_p, NULL, fmt);
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
 *		- int if operspy or not
 *		- format options
 * output	- NONE
 * side effects - do a global scan of all clients looking for match
 *		  this is slightly expensive on EFnet ...
 *		  marks assumed cleared for all clients initially
 *		  and will be left cleared on return
 */
static void
who_global(struct Client *source_p, const char *mask, int server_oper, int operspy, struct who_format *fmt)
{
	struct membership *msptr;
	struct Client *target_p;
	rb_dlink_node *lp, *ptr;
	int maxmatches = 500;

	/* first, list all matching INvisible clients on common channels
	 * if this is not an operspy who
	 */
	if(!operspy)
	{
		RB_DLINK_FOREACH(lp, source_p->user->channel.head)
		{
			msptr = lp->data;
			who_common_channel(source_p, msptr->chptr, mask, server_oper, &maxmatches, fmt);
		}
	}
	else if (!ConfigFileEntry.operspy_dont_care_user_info)
		report_operspy(source_p, "WHO", mask);

	/* second, list all matching visible clients and clear all marks
	 * on invisible clients
	 * if this is an operspy who, list all matching clients, no need
	 * to clear marks
	 */
	RB_DLINK_FOREACH(ptr, global_client_list.head)
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
					match(mask, target_p->host) || match(mask, target_p->servptr->name) ||
					(IsOper(source_p) && match(mask, target_p->orighost)) ||
					match(mask, target_p->info))
			{
				do_who(source_p, target_p, NULL, fmt);
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
 *		- format options
 * output	- NONE
 * side effects - do a who on given channel
 */
static void
do_who_on_channel(struct Client *source_p, struct Channel *chptr,
		  int server_oper, int member, struct who_format *fmt)
{
	struct Client *target_p;
	struct membership *msptr;
	rb_dlink_node *ptr;

	RB_DLINK_FOREACH(ptr, chptr->members.head)
	{
		msptr = ptr->data;
		target_p = msptr->client_p;

		if(server_oper && !IsOper(target_p))
			continue;

		if(member || !IsInvisible(target_p))
			do_who(source_p, target_p, msptr, fmt);
	}
}

/*
 * append_format
 *
 * inputs	- pointer to buffer
 *		- size of buffer
 *		- pointer to position
 *		- format string
 *		- arguments for format
 * output	- NONE
 * side effects - position incremented, possibly beyond size of buffer
 *		  this allows detecting overflow
 */
static void
append_format(char *buf, size_t bufsize, size_t *pos, const char *fmt, ...)
{
	size_t max, result;
	va_list ap;

	max = *pos >= bufsize ? 0 : bufsize - *pos;
	va_start(ap, fmt);
	result = rb_vsnprintf(buf + *pos, max, fmt, ap);
	va_end(ap);
	*pos += result;
}

/*
 * do_who
 *
 * inputs	- pointer to client requesting who
 *		- pointer to client to do who on
 *		- channel membership or NULL
 *		- format options
 * output	- NONE
 * side effects - do a who on given person
 */

static void
do_who(struct Client *source_p, struct Client *target_p, struct membership *msptr, struct who_format *fmt)
{
	char status[16];
	char str[510 + 1]; /* linebuf.c will add \r\n */
	size_t pos;
	const char *q;

	rb_sprintf(status, "%c%s%s",
		   target_p->user->away ? 'G' : 'H', IsOper(target_p) ? "*" : "", msptr ? find_channel_status(msptr, fmt->fields || IsCapable(source_p, CLICAP_MULTI_PREFIX)) : "");

	if (fmt->fields == 0)
		sendto_one(source_p, form_str(RPL_WHOREPLY), me.name,
			   source_p->name, msptr ? msptr->chptr->chname : "*",
			   target_p->username, target_p->host,
			   target_p->servptr->name, target_p->name, status,
			   ConfigServerHide.flatten_links && !IsOper(source_p) && !IsExemptShide(source_p) ? 0 : target_p->hopcount, 
			   target_p->info);
	else
	{
		str[0] = '\0';
		pos = 0;
		append_format(str, sizeof str, &pos, ":%s %d %s",
				me.name, RPL_WHOSPCRPL, source_p->name);
		if (fmt->fields & FIELD_QUERYTYPE)
			append_format(str, sizeof str, &pos, " %s", fmt->querytype);
		if (fmt->fields & FIELD_CHANNEL)
			append_format(str, sizeof str, &pos, " %s", msptr ? msptr->chptr->chname : "*");
		if (fmt->fields & FIELD_USER)
			append_format(str, sizeof str, &pos, " %s", target_p->username);
		if (fmt->fields & FIELD_IP)
		{
			if (show_ip(source_p, target_p) && !EmptyString(target_p->sockhost) && strcmp(target_p->sockhost, "0"))
				append_format(str, sizeof str, &pos, " %s", target_p->sockhost);
			else
				append_format(str, sizeof str, &pos, " %s", "255.255.255.255");
		}
		if (fmt->fields & FIELD_HOST)
			append_format(str, sizeof str, &pos, " %s", target_p->host);
		if (fmt->fields & FIELD_SERVER)
			append_format(str, sizeof str, &pos, " %s", target_p->servptr->name);
		if (fmt->fields & FIELD_NICK)
			append_format(str, sizeof str, &pos, " %s", target_p->name);
		if (fmt->fields & FIELD_FLAGS)
			append_format(str, sizeof str, &pos, " %s", status);
		if (fmt->fields & FIELD_HOP)
			append_format(str, sizeof str, &pos, " %d", ConfigServerHide.flatten_links && !IsOper(source_p) && !IsExemptShide(source_p) ? 0 : target_p->hopcount);
		if (fmt->fields & FIELD_IDLE)
			append_format(str, sizeof str, &pos, " %d", (int)(MyClient(target_p) ? rb_current_time() - target_p->localClient->last : 0));
		if (fmt->fields & FIELD_ACCOUNT)
		{
			/* display as in whois */
			q = target_p->user->suser;
			if (!EmptyString(q))
			{
				while(IsDigit(*q))
					q++;
				if(*q == '\0')
					q = target_p->user->suser;
			}
			else
				q = "0";
			append_format(str, sizeof str, &pos, " %s", q);
		}
		if (fmt->fields & FIELD_OPLEVEL)
			append_format(str, sizeof str, &pos, " %s", is_chanop(msptr) ? "999" : "n/a");
		if (fmt->fields & FIELD_INFO)
			append_format(str, sizeof str, &pos, " :%s", target_p->info);

		if (pos >= sizeof str)
		{
			static int warned = 0;
			if (!warned)
				sendto_realops_snomask(SNO_DEBUG, L_NETWIDE,
						"WHOX overflow while sending information about %s to %s",
						target_p->name, source_p->name);
			warned = 1;
		}
		sendto_one(source_p, "%s", str);
	}
}
