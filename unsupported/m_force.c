/* contrib/m_force.c
 * Copyright (C) 1996-2002 Hybrid Development Team
 * Copyright (C) 2004 ircd-ratbox Development Team
 *
 *  Redistribution and use in source and binary forms, with or without
 *  modification, are permitted provided that the following conditions are
 *  met:
 *
 *  1.Redistributions of source code must retain the above copyright notice,
 *    this list of conditions and the following disclaimer.
 *  2.Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *  3.The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 *  THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 *  IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 *  WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 *  DISCLAIMED. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT,
 *  INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 *  (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 *  SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 *  HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 *  STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING
 *  IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 *  POSSIBILITY OF SUCH DAMAGE.
 *
 * $Id: m_force.c 1425 2006-05-23 16:41:33Z jilles $
 */

#include "stdinc.h"
#include "tools.h"
#include "channel.h"
#include "class.h"
#include "client.h"
#include "common.h"
#include "irc_string.h"
#include "sprintf_irc.h"
#include "ircd.h"
#include "hostmask.h"
#include "numeric.h"
#include "commio.h"
#include "s_conf.h"
#include "s_newconf.h"
#include "s_log.h"
#include "send.h"
#include "hash.h"
#include "s_serv.h"
#include "msg.h"
#include "parse.h"
#include "modules.h"
#include "event.h"


static int mo_forcejoin(struct Client *client_p, struct Client *source_p,
			int parc, const char *parv[]);
static int mo_forcepart(struct Client *client_p, struct Client *source_p,
			int parc, const char *parv[]);

struct Message forcejoin_msgtab = {
	"FORCEJOIN", 0, 0, 0, MFLG_SLOW,
	{mg_unreg, mg_not_oper, mg_ignore, mg_ignore, mg_ignore, {mo_forcejoin, 3}}
};

struct Message forcepart_msgtab = {
	"FORCEPART", 0, 0, 0, MFLG_SLOW,
	{mg_unreg, mg_not_oper, mg_ignore, mg_ignore, mg_ignore, {mo_forcepart, 3}}
};

mapi_clist_av1 force_clist[] = { &forcejoin_msgtab, &forcepart_msgtab, NULL };

DECLARE_MODULE_AV1(force, NULL, NULL, force_clist, NULL, NULL, "$Revision: 1425 $");

/*
 * m_forcejoin
 *      parv[0] = sender prefix
 *      parv[1] = user to force
 *      parv[2] = channel to force them into
 */
static int
mo_forcejoin(struct Client *client_p, struct Client *source_p, int parc, const char *parv[])
{
	struct Client *target_p;
	struct Channel *chptr;
	int type;
	char mode;
	char sjmode;
	char *newch;

	if(!IsOperAdmin(source_p))
	{
		sendto_one(source_p, form_str(ERR_NOPRIVS), me.name, source_p->name, "forcejoin");
		return 0;
	}

	if((hunt_server(client_p, source_p, ":%s FORCEJOIN %s %s", 1, parc, parv)) != HUNTED_ISME)
		return 0;

	/* if target_p is not existant, print message
	 * to source_p and bail - scuzzy
	 */
	if((target_p = find_client(parv[1])) == NULL)
	{
		sendto_one(source_p, form_str(ERR_NOSUCHNICK), me.name, source_p->name, parv[1]);
		return 0;
	}

	if(!IsPerson(target_p))
		return 0;

	sendto_wallops_flags(UMODE_WALLOP, &me,
			     "FORCEJOIN called for %s %s by %s!%s@%s",
			     parv[1], parv[2], source_p->name, source_p->username, source_p->host);
	ilog(L_MAIN, "FORCEJOIN called for %s %s by %s!%s@%s",
	     parv[1], parv[2], source_p->name, source_p->username, source_p->host);
	sendto_server(NULL, NULL, NOCAPS, NOCAPS,
			":%s WALLOPS :FORCEJOIN called for %s %s by %s!%s@%s",
			me.name, parv[1], parv[2],
			source_p->name, source_p->username, source_p->host);

	/* select our modes from parv[2] if they exist... (chanop) */
	if(*parv[2] == '@')
	{
		type = CHFL_CHANOP;
		mode = 'o';
		sjmode = '@';
	}
	else if(*parv[2] == '+')
	{
		type = CHFL_VOICE;
		mode = 'v';
		sjmode = '+';
	}
	else
	{
		type = CHFL_PEON;
		mode = sjmode = '\0';
	}

	if(mode != '\0')
		parv[2]++;

	if((chptr = find_channel(parv[2])) != NULL)
	{
		if(IsMember(target_p, chptr))
		{
			/* debugging is fun... */
			sendto_one(source_p, ":%s NOTICE %s :*** Notice -- %s is already in %s",
				   me.name, source_p->name, target_p->name, chptr->chname);
			return 0;
		}

		add_user_to_channel(chptr, target_p, type);

		sendto_server(target_p, chptr, NOCAPS, NOCAPS,
			      ":%s SJOIN %ld %s + :%c%s",
			      me.name, (long) chptr->channelts,
			      chptr->chname, type ? sjmode : ' ', target_p->name);

		sendto_channel_local(ALL_MEMBERS, chptr, ":%s!%s@%s JOIN :%s",
				     target_p->name, target_p->username,
				     target_p->host, chptr->chname);

		if(type)
			sendto_channel_local(ALL_MEMBERS, chptr, ":%s MODE %s +%c %s",
					     me.name, chptr->chname, mode, target_p->name);

		if(chptr->topic != NULL)
		{
			sendto_one(target_p, form_str(RPL_TOPIC), me.name,
				   target_p->name, chptr->chname, chptr->topic);
			sendto_one(target_p, form_str(RPL_TOPICWHOTIME),
				   me.name, source_p->name, chptr->chname,
				   chptr->topic_info, chptr->topic_time);
		}

		channel_member_names(chptr, target_p, 1);
	}
	else
	{
		newch = LOCAL_COPY(parv[2]);
		if(!check_channel_name(newch))
		{
			sendto_one(source_p, form_str(ERR_BADCHANNAME), me.name,
				   source_p->name, (unsigned char *) newch);
			return 0;
		}

		/* channel name must begin with & or # */
		if(!IsChannelName(newch))
		{
			sendto_one(source_p, form_str(ERR_BADCHANNAME), me.name,
				   source_p->name, (unsigned char *) newch);
			return 0;
		}

		/* newch can't be longer than CHANNELLEN */
		if(strlen(newch) > CHANNELLEN)
		{
			sendto_one(source_p, ":%s NOTICE %s :Channel name is too long", me.name,
				   source_p->name);
			return 0;
		}

		chptr = get_or_create_channel(target_p, newch, NULL);
		add_user_to_channel(chptr, target_p, CHFL_CHANOP);

		/* send out a join, make target_p join chptr */
		sendto_server(target_p, chptr, NOCAPS, NOCAPS,
			      ":%s SJOIN %ld %s +nt :@%s", me.name,
			      (long) chptr->channelts, chptr->chname, target_p->name);

		sendto_channel_local(ALL_MEMBERS, chptr, ":%s!%s@%s JOIN :%s",
				     target_p->name, target_p->username,
				     target_p->host, chptr->chname);

		chptr->mode.mode |= MODE_TOPICLIMIT;
		chptr->mode.mode |= MODE_NOPRIVMSGS;

		sendto_channel_local(ALL_MEMBERS, chptr, ":%s MODE %s +nt", me.name, chptr->chname);

		target_p->localClient->last_join_time = CurrentTime;
		channel_member_names(chptr, target_p, 1);

		/* we do this to let the oper know that a channel was created, this will be
		 * seen from the server handling the command instead of the server that
		 * the oper is on.
		 */
		sendto_one(source_p, ":%s NOTICE %s :*** Notice -- Creating channel %s", me.name,
			   source_p->name, chptr->chname);
	}
	return 0;
}


static int
mo_forcepart(struct Client *client_p, struct Client *source_p, int parc, const char *parv[])
{
	struct Client *target_p;
	struct Channel *chptr;
	struct membership *msptr;

	if(!IsOperAdmin(source_p))
	{
		sendto_one(source_p, form_str(ERR_NOPRIVS), me.name, source_p->name, "forcepart");
		return 0;
	}

	if((hunt_server(client_p, source_p, ":%s FORCEPART %s %s", 1, parc, parv)) != HUNTED_ISME)
		return 0;

	/* if target_p == NULL then let the oper know */
	if((target_p = find_client(parv[1])) == NULL)
	{
		sendto_one(source_p, form_str(ERR_NOSUCHNICK), me.name, source_p->name, parv[1]);
		return 0;
	}

	if(!IsClient(target_p))
		return 0;

	sendto_wallops_flags(UMODE_WALLOP, &me,
			     "FORCEPART called for %s %s by %s!%s@%s",
			     parv[1], parv[2], source_p->name, source_p->username, source_p->host);
	ilog(L_MAIN, "FORCEPART called for %s %s by %s!%s@%s",
	     parv[1], parv[2], source_p->name, source_p->username, source_p->host);
	sendto_server(NULL, NULL, NOCAPS, NOCAPS,
			":%s WALLOPS :FORCEPART called for %s %s by %s!%s@%s",
			me.name, parv[1], parv[2],
			source_p->name, source_p->username, source_p->host);

	if((chptr = find_channel(parv[2])) == NULL)
	{
		sendto_one_numeric(source_p, ERR_NOSUCHCHANNEL,
				   form_str(ERR_NOSUCHCHANNEL), parv[1]);
		return 0;
	}

	if((msptr = find_channel_membership(chptr, target_p)) == NULL)
	{
		sendto_one_numeric(source_p, ERR_USERNOTINCHANNEL,
				form_str(ERR_USERNOTINCHANNEL),
				parv[1], parv[2]);
		return 0;
	}

	sendto_server(target_p, chptr, NOCAPS, NOCAPS,
		      ":%s PART %s :%s", target_p->name, chptr->chname, target_p->name);

	sendto_channel_local(ALL_MEMBERS, chptr, ":%s!%s@%s PART %s :%s",
			     target_p->name, target_p->username,
			     target_p->host, chptr->chname, target_p->name);


	remove_user_from_channel(msptr);

	return 0;
}
