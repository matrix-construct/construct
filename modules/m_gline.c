/*
 *  ircd-ratbox: A slightly useful ircd.
 *  m_gline.c: Votes towards globally banning a mask.
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
 *  $Id: m_gline.c 1146 2006-04-07 22:52:35Z jilles $
 */

#include "stdinc.h"
#include "tools.h"
#include "s_gline.h"
#include "channel.h"
#include "client.h"
#include "common.h"
#include "config.h"
#include "irc_string.h"
#include "sprintf_irc.h"
#include "ircd.h"
#include "hostmask.h"
#include "numeric.h"
#include "commio.h"
#include "s_conf.h"
#include "s_newconf.h"
#include "scache.h"
#include "send.h"
#include "msg.h"
#include "s_serv.h"
#include "hash.h"
#include "parse.h"
#include "modules.h"
#include "s_log.h"

static int mo_gline(struct Client *, struct Client *, int, const char **);
static int mc_gline(struct Client *, struct Client *, int, const char **);
static int ms_gline(struct Client *, struct Client *, int, const char **);
static int mo_ungline(struct Client *, struct Client *, int, const char **);

struct Message gline_msgtab = {
	"GLINE", 0, 0, 0, MFLG_SLOW,
	{mg_unreg, mg_not_oper, {mc_gline, 3}, {ms_gline, 7}, mg_ignore, {mo_gline, 3}}
};
struct Message ungline_msgtab = {
	"UNGLINE", 0, 0, 0, MFLG_SLOW,
	{mg_unreg, mg_not_oper, mg_ignore, mg_ignore, mg_ignore, {mo_ungline, 2}}
};

mapi_clist_av1 gline_clist[] = { &gline_msgtab, &ungline_msgtab, NULL };
DECLARE_MODULE_AV1(gline, NULL, NULL, gline_clist, NULL, NULL, "$Revision: 1146 $");

static int majority_gline(struct Client *source_p, const char *user,
			  const char *host, const char *reason);
static void set_local_gline(struct Client *source_p, const char *user,
			    const char *host, const char *reason);

static int check_wild_gline(const char *, const char *);
static int invalid_gline(struct Client *, const char *, const char *, char *);

static int remove_temp_gline(const char *, const char *);


/* mo_gline()
 *
 * inputs       - The usual for a m_ function
 * output       -
 * side effects - place a gline if 3 opers agree
 */
static int
mo_gline(struct Client *client_p, struct Client *source_p, int parc, const char *parv[])
{
	const char *user = NULL;
	char *host = NULL;	/* user and host of GLINE "victim" */
	char *reason = NULL;	/* reason for "victims" demise */
	char splat[] = "*";
	char *ptr;

	if(!ConfigFileEntry.glines)
	{
		sendto_one(source_p, ":%s NOTICE %s :GLINE disabled, perhaps you want a clustered or remote KLINE?",
			   me.name, source_p->name);
		return 0;
	}

	if(!IsOperGline(source_p))
	{
		sendto_one(source_p, form_str(ERR_NOPRIVS),
			   me.name, source_p->name, "gline");
		return 0;
	}

	host = strchr(parv[1], '@');

	/* specific user@host */
	if(host != NULL)
	{
		user = parv[1];
		*(host++) = '\0';

		/* gline for "@host", use *@host */
		if(*user == '\0')
			user = splat;
	}
	/* just a host? */
	else
	{
		/* ok, its not a host.. abort */
		if(strchr(parv[1], '.') == NULL)
		{
			sendto_one(source_p,
				   ":%s NOTICE %s :Invalid parameters",
				   me.name, source_p->name);
			return 0;
		}

		user = splat;
		host = LOCAL_COPY(parv[1]);
	}

	reason = LOCAL_COPY(parv[2]);

	if(invalid_gline(source_p, user, host, reason))
		return 0;

	/* Not enough non-wild characters were found, assume they are trying to gline *@*. */
	if(check_wild_gline(user, host))
	{
		if(MyClient(source_p))
			sendto_one(source_p,
				   ":%s NOTICE %s :Please include at least %d non-wildcard "
				   "characters with the user@host",
				   me.name, source_p->name, 
				   ConfigFileEntry.min_nonwildcard);
		return 0;
	}

	if((ptr = strchr(host, '/')) != NULL)
	{
		int bitlen;
		bitlen = strtol(++ptr, NULL, 10);

		/* ipv4? */
		if(strchr(host, ':') == NULL)
		{
			if(bitlen < ConfigFileEntry.gline_min_cidr)
			{
				sendto_one(source_p, ":%s NOTICE %s :Cannot set G-Lines with cidr length < %d",
					   me.name, source_p->name,
					   ConfigFileEntry.gline_min_cidr);
				return 0;
			}
		}
		/* ipv6 */
		else if(bitlen < ConfigFileEntry.gline_min_cidr6)
		{
			sendto_one(source_p, ":%s NOTICE %s :Cannot set G-Lines with cidr length < %d",
				   me.name, source_p->name, 
				   ConfigFileEntry.gline_min_cidr6);
			return 0;
		}
	}

	/* inform users about the gline before we call majority_gline()
	 * so already voted comes below gline request --fl
	 */
	sendto_realops_snomask(SNO_GENERAL, L_ALL,
			"%s!%s@%s on %s is requesting gline for [%s@%s] [%s]",
			source_p->name, source_p->username,
			source_p->host, me.name, user, host, reason);
	ilog(L_GLINE, "R %s %s %s %s %s %s %s",
	     source_p->name, source_p->username, source_p->host, 
	     source_p->user->server, user, host, reason);

	/* If at least 3 opers agree this user should be G lined then do it */
	majority_gline(source_p, user, host, reason);

	/* 4 param version for hyb-7 servers */
	sendto_server(NULL, NULL, CAP_GLN|CAP_TS6, NOCAPS,
			":%s GLINE %s %s :%s", 
			use_id(source_p), user, host, reason);
	sendto_server(NULL, NULL, CAP_GLN, CAP_TS6,
			":%s GLINE %s %s :%s", 
			source_p->name, user, host, reason);

	/* 8 param for hyb-6 */
	sendto_server(NULL, NULL, NOCAPS, CAP_GLN,
			":%s GLINE %s %s %s %s %s %s :%s",
			me.name, source_p->name, source_p->username,
			source_p->host, source_p->user->server, 
			user, host, reason);
	return 0;
}

/* mc_gline()
 */
static int
mc_gline(struct Client *client_p, struct Client *source_p,
	 int parc, const char *parv[])
{
	struct Client *acptr;
	const char *user;
	const char *host;
	char *reason;
	char *ptr;

	/* hyb6 allows empty gline reasons */
	if(parc < 4 || EmptyString(parv[3]))
		return 0;

	acptr = source_p;

	user = parv[1];
	host = parv[2];
	reason = LOCAL_COPY(parv[3]);

	if(invalid_gline(acptr, user, host, reason))
		return 0;

	sendto_server(client_p, NULL, CAP_GLN|CAP_TS6, NOCAPS,
		      ":%s GLINE %s %s :%s",
		      use_id(acptr), user, host, reason);
	sendto_server(client_p, NULL, CAP_GLN, CAP_TS6,
		      ":%s GLINE %s %s :%s",
		      acptr->name, user, host, reason);
	sendto_server(client_p, NULL, NOCAPS, CAP_GLN,
		      ":%s GLINE %s %s %s %s %s %s :%s",
		      acptr->user->server, acptr->name, 
		      acptr->username, acptr->host,
		      acptr->user->server, user, host, reason);

	if(!ConfigFileEntry.glines)
		return 0;

	/* check theres enough non-wildcard chars */
	if(check_wild_gline(user, host))
	{
		sendto_realops_snomask(SNO_GENERAL, L_ALL,
				"%s!%s@%s on %s is requesting a gline without "
				"%d non-wildcard characters for [%s@%s] [%s]",
				acptr->name, acptr->username, 
				acptr->host, acptr->user->server,
				ConfigFileEntry.min_nonwildcard,
				user, host, reason);
		return 0;
	}

	if((ptr = strchr(host, '/')) != NULL)
	{
		int bitlen;
		bitlen = strtol(++ptr, NULL, 10);

		/* ipv4? */
		if(strchr(host, ':') == NULL)
		{
			if(bitlen < ConfigFileEntry.gline_min_cidr)
			{
				sendto_realops_snomask(SNO_GENERAL, L_ALL, "%s!%s@%s on %s is requesting a "
						     "gline with a cidr mask < %d for [%s@%s] [%s]",
						     acptr->name, acptr->username, acptr->host,
						     acptr->user->server,
						     ConfigFileEntry.gline_min_cidr, 
						     user, host, reason);
				return 0;
			}
		}
		/* ipv6 */
		else if(bitlen < ConfigFileEntry.gline_min_cidr6)
		{
			sendto_realops_snomask(SNO_GENERAL, L_ALL, "%s!%s@%s on %s is requesting a "
					     "gline with a cidr mask < %d for [%s@%s] [%s]",
					     acptr->name, acptr->username, acptr->host,
					     acptr->user->server,
					     ConfigFileEntry.gline_min_cidr6,
					     user, host, reason);
			return 0;
		}
	}


	sendto_realops_snomask(SNO_GENERAL, L_ALL,
			"%s!%s@%s on %s is requesting gline for [%s@%s] [%s]",
			acptr->name, acptr->username, acptr->host,
			acptr->user->server, user, host, reason);

	ilog(L_GLINE, "R %s %s %s %s %s %s %s",
	     source_p->name, source_p->username, source_p->host, 
	     source_p->user->server, user, host, reason);

	/* If at least 3 opers agree this user should be G lined then do it */
	majority_gline(acptr, user, host, reason);

	return 0;
}


/* ms_gline()
 *
 * inputs       - The usual for a m_ function
 * output       -
 * side effects - attempts to place a gline, if 3 opers agree
 */
static int
ms_gline(struct Client *client_p, struct Client *source_p, int parc, const char *parv[])
{
	struct Client *acptr;
	const char *user;
	const char *host;
	char *reason;

	/* hyb6 allows empty gline reasons */
	if(parc < 8 || EmptyString(parv[7]))
		return 0;

	/* client doesnt exist.. someones messing */
	if((acptr = find_client(parv[1])) == NULL)
		return 0;

	/* client that sent the gline, isnt on the server that sent
	 * the gline out.  somethings fucked.
	 */
	if(acptr->servptr != source_p)
		return 0;

	user = parv[5];
	host = parv[6];
	reason = LOCAL_COPY(parv[7]);

	if(invalid_gline(acptr, user, host, reason))
		return 0;

	sendto_server(client_p, NULL, CAP_GLN|CAP_TS6, NOCAPS,
		      ":%s GLINE %s %s :%s",
		      use_id(acptr), user, host, reason);
	sendto_server(client_p, NULL, CAP_GLN, CAP_TS6,
		      ":%s GLINE %s %s :%s",
		      acptr->name, user, host, reason);
	sendto_server(client_p, NULL, NOCAPS, CAP_GLN,
		      ":%s GLINE %s %s %s %s %s %s :%s",
		      acptr->user->server, acptr->name, 
		      acptr->username, acptr->host,
		      acptr->user->server, user, host, reason);

	if(!ConfigFileEntry.glines)
		return 0;

	/* check theres enough non-wildcard chars */
	if(check_wild_gline(user, host))
	{
		sendto_realops_snomask(SNO_GENERAL, L_ALL,
				"%s!%s@%s on %s is requesting a gline without "
				"%d non-wildcard characters for [%s@%s] [%s]",
				acptr->name, acptr->username, 
				acptr->host, acptr->user->server,
				ConfigFileEntry.min_nonwildcard,
				user, host, reason);
		return 0;
	}

	sendto_realops_snomask(SNO_GENERAL, L_ALL,
			"%s!%s@%s on %s is requesting gline for [%s@%s] [%s]",
			acptr->name, acptr->username, acptr->host,
			acptr->user->server, user, host, reason);

	ilog(L_GLINE, "R %s %s %s %s %s %s %s",
	     acptr->name, acptr->username, acptr->host, 
	     acptr->user->server, user, host, reason);

	/* If at least 3 opers agree this user should be G lined then do it */
	majority_gline(acptr, user, host, reason);

	return 0;
}

/* mo_ungline()
 *
 *      parv[0] = sender nick
 *      parv[1] = gline to remove
 */
static int
mo_ungline(struct Client *client_p, struct Client *source_p, int parc, const char *parv[])
{
	const char *user;
	char *h = LOCAL_COPY(parv[1]);
	char *host;
	char splat[] = "*";

	if(!ConfigFileEntry.glines)
	{
		sendto_one(source_p, ":%s NOTICE %s :UNGLINE disabled, perhaps you want UNKLINE?", me.name, parv[0]);
		return 0;
	}

	if(!IsOperUnkline(source_p) || !IsOperGline(source_p))
	{
		sendto_one(source_p, form_str(ERR_NOPRIVS),
			   me.name, source_p->name, "unkline");
		return 0;
	}

	if((host = strchr(h, '@')) || *h == '*')
	{
		/* Explicit user@host mask given */

		if(host)
		{
			*host++ = '\0';
	
			/* check for @host */
			if(*h)
				user = h;
			else
				user = splat;

			if(!*host)
				host = splat;
		}
		else
		{
			user = splat;
			host = h;
		}
	}
	else
	{
		sendto_one(source_p, ":%s NOTICE %s :Invalid parameters", me.name, parv[0]);
		return 0;
	}

	if(remove_temp_gline(user, host))
	{
		sendto_one(source_p, ":%s NOTICE %s :Un-glined [%s@%s]",
			   me.name, parv[0], user, host);
		sendto_realops_snomask(SNO_GENERAL, L_ALL,
				     "%s has removed the G-Line for: [%s@%s]",
				     get_oper_name(source_p), user, host);
		ilog(L_GLINE, "U %s %s %s %s %s %s",
		     source_p->name, source_p->username, source_p->host, 
		     source_p->user->server, user, host);
	}
	else
	{
		sendto_one(source_p, ":%s NOTICE %s :No G-Line for %s@%s",
			   me.name, parv[0], user, host);
	}

	return 0;
}

/*
 * check_wild_gline
 *
 * inputs       - user, host of gline
 * output       - 1 if not enough non-wildchar char's, 0 if ok
 * side effects - NONE
 */
static int
check_wild_gline(const char *user, const char *host)
{
	const char *p;
	char tmpch;
	int nonwild;

	nonwild = 0;
	p = user;

	while ((tmpch = *p++))
	{
		if(!IsKWildChar(tmpch))
		{
			/* enough of them, break */
			if(++nonwild >= ConfigFileEntry.min_nonwildcard)
				break;
		}
	}

	if(nonwild < ConfigFileEntry.min_nonwildcard)
	{
		/* user doesnt, try host */
		p = host;
		while ((tmpch = *p++))
		{
			if(!IsKWildChar(tmpch))
				if(++nonwild >= ConfigFileEntry.min_nonwildcard)
					break;
		}
	}

	if(nonwild < ConfigFileEntry.min_nonwildcard)
		return 1;
	else
		return 0;
}

/* invalid_gline
 *
 * inputs	- pointer to source client, ident, host and reason
 * outputs	- 1 if invalid, 0 if valid
 * side effects -
 */
static int
invalid_gline(struct Client *source_p, const char *luser,
	      const char *lhost, char *lreason)
{
	if(strchr(luser, '!'))
	{
		sendto_one(source_p, ":%s NOTICE %s :Invalid character '!' in gline",
			   me.name, source_p->name);
		return 1;
	}

	if(strlen(lreason) > REASONLEN)
		lreason[REASONLEN] = '\0';

	return 0;
}

/*
 * set_local_gline
 *
 * inputs	- pointer to oper nick/username/host/server,
 * 		  victim user/host and reason
 * output	- NONE
 * side effects	-
 */
static void
set_local_gline(struct Client *source_p, const char *user,
		const char *host, const char *reason)
{
	char buffer[IRCD_BUFSIZE];
	struct ConfItem *aconf;
	const char *current_date;
	char *my_reason;
	char *oper_reason;

	current_date = smalldate();

	my_reason = LOCAL_COPY(reason);

	aconf = make_conf();
	aconf->status = CONF_GLINE;
	aconf->flags |= CONF_FLAGS_TEMPORARY;

	if(strlen(my_reason) > REASONLEN)
		my_reason[REASONLEN-1] = '\0';

	if((oper_reason = strchr(my_reason, '|')) != NULL)
	{
		*oper_reason = '\0';
		oper_reason++;

		if(!EmptyString(oper_reason))
			DupString(aconf->spasswd, oper_reason);
	}

	ircsnprintf(buffer, sizeof(buffer), "%s (%s)", reason, current_date);

	DupString(aconf->passwd, buffer);
	DupString(aconf->user, user);
	DupString(aconf->host, host);
	aconf->hold = CurrentTime + ConfigFileEntry.gline_time;
	add_gline(aconf);

	sendto_realops_snomask(SNO_GENERAL, L_ALL,
			     "%s!%s@%s on %s has triggered gline for [%s@%s] [%s]",
			     source_p->name, source_p->username,
			     source_p->host, source_p->user->server,
			     user, host, reason);
	ilog(L_GLINE, "T %s %s %s %s %s %s %s",
	     source_p->name, source_p->username, source_p->host, 
	     source_p->user->server, user, host, reason);

	check_glines();
}

/* majority_gline()
 *
 * input	- client doing gline, user, host and reason of gline
 * output       - YES if there are 3 different opers/servers agree, else NO
 * side effects -
 */
static int
majority_gline(struct Client *source_p, const char *user,
	       const char *host, const char *reason)
{
	dlink_node *pending_node;
	struct gline_pending *pending;

	/* to avoid desync.. --fl */
	cleanup_glines(NULL);

	/* if its already glined, why bother? :) -- fl_ */
	if(find_is_glined(host, user))
		return NO;

	DLINK_FOREACH(pending_node, pending_glines.head)
	{
		pending = pending_node->data;

		if((irccmp(pending->user, user) == 0) &&
		   (irccmp(pending->host, host) == 0))
		{
			/* check oper or server hasnt already voted */
			if(((irccmp(pending->oper_user1, source_p->username) == 0) ||
			    (irccmp(pending->oper_host1, source_p->host) == 0)))
			{
				sendto_realops_snomask(SNO_GENERAL, L_ALL, "oper has already voted");
				return NO;
			}
			else if(irccmp(pending->oper_server1, source_p->user->server) == 0)
			{
				sendto_realops_snomask(SNO_GENERAL, L_ALL, "server has already voted");
				return NO;
			}

			if(pending->oper_user2[0] != '\0')
			{
				/* if two other opers on two different servers have voted yes */
				if(((irccmp(pending->oper_user2, source_p->username) == 0) ||
				    (irccmp(pending->oper_host2, source_p->host) == 0)))
				{
					sendto_realops_snomask(SNO_GENERAL, L_ALL,
							     "oper has already voted");
					return NO;
				}
				else if(irccmp(pending->oper_server2, source_p->user->server) == 0)
				{
					sendto_realops_snomask(SNO_GENERAL, L_ALL,
							     "server has already voted");
					return NO;
				}

				/* trigger the gline using the original reason --fl */
				set_local_gline(source_p, user, host,
						pending->reason1);

				cleanup_glines(NULL);
				return YES;
			}
			else
			{
				strlcpy(pending->oper_nick2, source_p->name,
					sizeof(pending->oper_nick2));
				strlcpy(pending->oper_user2, source_p->username,
					sizeof(pending->oper_user2));
				strlcpy(pending->oper_host2, source_p->host,
					sizeof(pending->oper_host2));
				DupString(pending->reason2, reason);
				pending->oper_server2 = find_or_add(source_p->user->server);
				pending->last_gline_time = CurrentTime;
				pending->time_request2 = CurrentTime;
				return NO;
			}
		}
	}

	/* no pending gline, create a new one */
	pending = (struct gline_pending *) 
			    MyMalloc(sizeof(struct gline_pending));

	strlcpy(pending->oper_nick1, source_p->name,
		sizeof(pending->oper_nick1));
	strlcpy(pending->oper_user1, source_p->username,
		sizeof(pending->oper_user1));
	strlcpy(pending->oper_host1, source_p->host,
		sizeof(pending->oper_host1));

	pending->oper_server1 = find_or_add(source_p->user->server);

	strlcpy(pending->user, user, sizeof(pending->user));
	strlcpy(pending->host, host, sizeof(pending->host));
	DupString(pending->reason1, reason);
	pending->reason2 = NULL;

	pending->last_gline_time = CurrentTime;
	pending->time_request1 = CurrentTime;

	dlinkAddAlloc(pending, &pending_glines);

	return NO;
}

/* remove_temp_gline()
 *
 * inputs       - username, hostname to ungline
 * outputs      -
 * side effects - tries to ungline anything that matches
 */
static int
remove_temp_gline(const char *user, const char *host)
{
	struct ConfItem *aconf;
	dlink_node *ptr;
	struct irc_sockaddr_storage addr, caddr;
	int bits, cbits;
	int mtype, gtype;

	mtype = parse_netmask(host, (struct sockaddr *)&addr, &bits);

	DLINK_FOREACH(ptr, glines.head)
	{
		aconf = ptr->data;

		gtype = parse_netmask(aconf->host, (struct sockaddr *)&caddr, &cbits);

		if(gtype != mtype || (user && irccmp(user, aconf->user)))
			continue;

		if(gtype == HM_HOST)
		{
			if(irccmp(aconf->host, host))
				continue;
		}
		else if(bits != cbits ||
			!comp_with_mask_sock((struct sockaddr *)&addr, 
						(struct sockaddr *)&caddr, bits))
			continue;

		dlinkDestroy(ptr, &glines);
		delete_one_address_conf(aconf->host, aconf);
		return YES;
	}

	return NO;
}
