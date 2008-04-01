/*
 *  ircd-ratbox: an advanced Internet Relay Chat Daemon(ircd).
 *  m_cmessage.c: Handles CPRIVMSG/CNOTICE, target change limitation free
 *                PRIVMSG/NOTICE implementations.
 *
 *  Copyright (C) 2005 Lee Hardy <lee -at- leeh.co.uk>
 *  Copyright (C) 2005 ircd-ratbox development team
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
 *  $Id: m_cmessage.c 1543 2006-06-01 18:18:28Z jilles $
 */
#include "stdinc.h"
#include "client.h"
#include "channel.h"
#include "numeric.h"
#include "msg.h"
#include "modules.h"
#include "hash.h"
#include "send.h"
#include "s_conf.h"
#include "packet.h"

static int m_cmessage(int, const char *, struct Client *, struct Client *, int, const char **);
static int m_cprivmsg(struct Client *, struct Client *, int, const char **);
static int m_cnotice(struct Client *, struct Client *, int, const char **);

struct Message cprivmsg_msgtab = {
	"CPRIVMSG", 0, 0, 0, MFLG_SLOW,
	{mg_ignore, {m_cprivmsg, 4}, mg_ignore, mg_ignore, mg_ignore, {m_cprivmsg, 4}}
};
struct Message cnotice_msgtab = {
	"CNOTICE", 0, 0, 0, MFLG_SLOW,
	{mg_ignore, {m_cnotice, 4}, mg_ignore, mg_ignore, mg_ignore, {m_cnotice, 4}}
};

mapi_clist_av1 cmessage_clist[] = { &cprivmsg_msgtab, &cnotice_msgtab, NULL };
DECLARE_MODULE_AV1(cmessage, NULL, NULL, cmessage_clist, NULL, NULL, "$Revision: 1543 $");

#define PRIVMSG 0
#define NOTICE 1

static int
m_cprivmsg(struct Client *client_p, struct Client *source_p, int parc, const char *parv[])
{
	return m_cmessage(PRIVMSG, "PRIVMSG", client_p, source_p, parc, parv);
}

static int
m_cnotice(struct Client *client_p, struct Client *source_p, int parc, const char *parv[])
{
	return m_cmessage(NOTICE, "NOTICE", client_p, source_p, parc, parv);
}

static int
m_cmessage(int p_or_n, const char *command,
		struct Client *client_p, struct Client *source_p, int parc, const char *parv[])
{
	struct Client *target_p;
	struct Channel *chptr;
	struct membership *msptr;

	if(!IsFloodDone(source_p))
		flood_endgrace(source_p);

	if((target_p = find_named_person(parv[1])) == NULL)
	{
		if(p_or_n != NOTICE)
			sendto_one_numeric(source_p, ERR_NOSUCHNICK,
					form_str(ERR_NOSUCHNICK), parv[1]);
		return 0;
	}

	if((chptr = find_channel(parv[2])) == NULL)
	{
		if(p_or_n != NOTICE)
			sendto_one_numeric(source_p, ERR_NOSUCHCHANNEL,
					form_str(ERR_NOSUCHCHANNEL), parv[2]);
		return 0;
	}

	if((msptr = find_channel_membership(chptr, source_p)) == NULL)
	{
		if(p_or_n != NOTICE)
			sendto_one_numeric(source_p, ERR_NOTONCHANNEL,
					form_str(ERR_NOTONCHANNEL), 
					chptr->chname);
		return 0;
	}

	if(!is_chanop_voiced(msptr))
	{
		if(p_or_n != NOTICE)
			sendto_one(source_p, form_str(ERR_VOICENEEDED),
				me.name, source_p->name, chptr->chname);
		return 0;
	}

	if(!IsMember(target_p, chptr))
	{
		if(p_or_n != NOTICE)
			sendto_one_numeric(source_p, ERR_USERNOTINCHANNEL,
					form_str(ERR_USERNOTINCHANNEL),
					target_p->name, chptr->chname);
		return 0;
	}

	if(MyClient(target_p) && (IsSetCallerId(target_p) || (IsSetRegOnlyMsg(target_p) && !source_p->user->suser[0])) &&
	   !accept_message(source_p, target_p) && !IsOper(source_p))
	{
		if (IsSetRegOnlyMsg(target_p) && !source_p->user->suser[0])
		{
			if (p_or_n != NOTICE)
				sendto_one_numeric(source_p, ERR_NONONREG,
						form_str(ERR_NONONREG),
						target_p->name);
			return 0;
		}
		if(p_or_n != NOTICE)
			sendto_one_numeric(source_p, ERR_TARGUMODEG,
					form_str(ERR_TARGUMODEG), target_p->name);

		if((target_p->localClient->last_caller_id_time +
		    ConfigFileEntry.caller_id_wait) < rb_current_time())
		{
			if(p_or_n != NOTICE)
				sendto_one_numeric(source_p, RPL_TARGNOTIFY,
						form_str(RPL_TARGNOTIFY),
						target_p->name);

			sendto_one(target_p, form_str(RPL_UMODEGMSG),
				me.name, target_p->name, source_p->name,
				source_p->username, source_p->host);

			target_p->localClient->last_caller_id_time = rb_current_time();
		}

		return 0;
	}

	if(p_or_n != NOTICE)
		source_p->localClient->last = rb_current_time();

	sendto_anywhere(target_p, source_p, command, ":%s", parv[3]);
	return 0;
}
