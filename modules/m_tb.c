/* modules/m_tb.c
 * 
 *  Copyright (C) 2003 Lee Hardy <lee@leeh.co.uk>
 *  Copyright (C) 2003-2005 ircd-ratbox development team
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 * 1.Redistributions of source code must retain the above copyright notice,
 *   this list of conditions and the following disclaimer.
 * 2.Redistributions in binary form must reproduce the above copyright
 *   notice, this list of conditions and the following disclaimer in the
 *   documentation and/or other materials provided with the distribution.
 * 3.The name of the author may not be used to endorse or promote products
 *   derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING
 * IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 *
 * $Id: m_tb.c 1349 2006-05-17 17:37:46Z jilles $
 */

#include "stdinc.h"
#include "send.h"
#include "channel.h"
#include "client.h"
#include "common.h"
#include "config.h"
#include "ircd.h"
#include "match.h"
#include "s_conf.h"
#include "msg.h"
#include "modules.h"
#include "hash.h"
#include "s_serv.h"

static int ms_tb(struct Client *client_p, struct Client *source_p, int parc, const char *parv[]);
static int ms_etb(struct Client *client_p, struct Client *source_p, int parc, const char *parv[]);

struct Message tb_msgtab = {
	"TB", 0, 0, 0, MFLG_SLOW,
	{mg_unreg, mg_ignore, mg_ignore, {ms_tb, 4}, mg_ignore, mg_ignore}
};

struct Message etb_msgtab = {
	"ETB", 0, 0, 0, MFLG_SLOW,
	{mg_unreg, mg_ignore, {ms_etb, 5}, {ms_etb, 5}, mg_ignore, mg_ignore}
};

mapi_clist_av1 tb_clist[] =  { &tb_msgtab, &etb_msgtab, NULL };
DECLARE_MODULE_AV1(tb, NULL, NULL, tb_clist, NULL, NULL, "$Revision: 1349 $");

/* m_tb()
 *
 * parv[1] - channel
 * parv[2] - topic ts
 * parv[3] - optional topicwho/topic
 * parv[4] - topic
 */
static int
ms_tb(struct Client *client_p, struct Client *source_p, int parc, const char *parv[])
{
	struct Channel *chptr;
	const char *newtopic;
	const char *newtopicwho;
	time_t newtopicts;
	struct Client *fakesource_p;

	chptr = find_channel(parv[1]);

	if(chptr == NULL)
		return 0;

	newtopicts = atol(parv[2]);

	/* Hide connecting server on netburst -- jilles */
	if (ConfigServerHide.flatten_links && !HasSentEob(source_p))
		fakesource_p = &me;
	else
		fakesource_p = source_p;

	if(parc == 5)
	{
		newtopic = parv[4];
		newtopicwho = parv[3];
	}
	else
	{
		newtopic = parv[3];
		newtopicwho = fakesource_p->name;
	}

	if (EmptyString(newtopic))
		return 0;

	if(chptr->topic == NULL || chptr->topic_time > newtopicts)
	{
		/* its possible the topicts is a few seconds out on some
		 * servers, due to lag when propagating it, so if theyre the
		 * same topic just drop the message --fl
		 */
		if(chptr->topic != NULL && strcmp(chptr->topic, newtopic) == 0)
			return 0;

		set_channel_topic(chptr, newtopic, newtopicwho, newtopicts);
		sendto_channel_local(ALL_MEMBERS, chptr, ":%s TOPIC %s :%s",
				     fakesource_p->name, chptr->chname, newtopic);
		sendto_server(client_p, chptr, CAP_TB|CAP_TS6, NOCAPS,
			      ":%s TB %s %ld %s%s:%s",
			      use_id(source_p), chptr->chname, (long) chptr->topic_time,
			      ConfigChannel.burst_topicwho ? chptr->topic_info : "",
			      ConfigChannel.burst_topicwho ? " " : "", chptr->topic);
	}

	return 0;
}

/* ms_etb()
 *
 * parv[1] - channel ts
 * parv[2] - channel
 * parv[3] - topic ts
 * parv[4] - topicwho
 * parv[5] - topic
 */
static int
ms_etb(struct Client *client_p, struct Client *source_p, int parc, const char *parv[])
{
	struct Channel *chptr;
	const char *newtopic;
	const char *newtopicwho;
	time_t channelts, newtopicts;
	struct Client *fakesource_p, *source_server_p;
	int textchange, can_use_tb, member;

	channelts = atol(parv[1]);
	chptr = find_channel(parv[2]);

	if(chptr == NULL)
		return 0;

	newtopicts = atol(parv[3]);

	/* Hide connecting server on netburst -- jilles */
	if (IsServer(source_p) && ConfigServerHide.flatten_links &&
			!HasSentEob(source_p))
		fakesource_p = &me;
	else
		fakesource_p = source_p;

	newtopicwho = parv[4];
	newtopic = parv[parc - 1];

	if(chptr->topic == NULL || chptr->channelts > channelts ||
			(chptr->channelts == channelts && chptr->topic_time < newtopicts))
	{
		textchange = chptr->topic == NULL || strcmp(chptr->topic, newtopic);
		can_use_tb = textchange && !EmptyString(newtopic) &&
			(chptr->topic == NULL || chptr->topic_time > newtopicts);

		set_channel_topic(chptr, newtopic, newtopicwho, newtopicts);
		newtopic = chptr->topic ? chptr->topic : "";
		if (chptr->topic_info)
			newtopicwho = chptr->topic_info;

		/* Do not send a textually identical topic to clients,
		 * but do propagate the new topicts/topicwho to servers.
		 */
		if(textchange)
		{
			if (IsPerson(fakesource_p))
				sendto_channel_local(ALL_MEMBERS, chptr,
						":%s!%s@%s TOPIC %s :%s",
						fakesource_p->name,
						fakesource_p->username,
						fakesource_p->host,
						chptr->chname,
						newtopic);
			else
				sendto_channel_local(ALL_MEMBERS, chptr,
						":%s TOPIC %s :%s",
						fakesource_p->name,
						chptr->chname, newtopic);
		}
		/* Propagate channelts as given, because an older channelts
		 * forces any change.
		 */
		sendto_server(client_p, chptr, CAP_EOPMOD|CAP_TS6, NOCAPS,
			      ":%s ETB %ld %s %ld %s :%s",
			      use_id(source_p), (long)channelts, chptr->chname,
			      (long)newtopicts, newtopicwho, newtopic);
		source_server_p = IsServer(source_p) ? source_p : source_p->servptr;
		if (can_use_tb)
			sendto_server(client_p, chptr, CAP_TB|CAP_TS6, CAP_EOPMOD,
				      ":%s TB %s %ld %s :%s",
				      use_id(source_server_p),
				      chptr->chname, (long)newtopicts,
				      newtopicwho, newtopic);
		else if (IsPerson(source_p) && textchange)
		{
			member = IsMember(source_p, chptr);
			if (!member)
				sendto_server(client_p, chptr, CAP_TS6, CAP_EOPMOD,
					      ":%s SJOIN %ld %s + :@%s",
					      use_id(source_server_p),
					      (long)chptr->channelts,
					      chptr->chname, use_id(source_p));
			if (EmptyString(newtopic) ||
					newtopicts >= rb_current_time() - 60)
				sendto_server(client_p, chptr, CAP_TS6, CAP_EOPMOD,
					      ":%s TOPIC %s :%s",
					      use_id(source_p),
					      chptr->chname, newtopic);
			else
			{
				sendto_server(client_p, chptr, CAP_TS6, CAP_EOPMOD,
					      ":%s TOPIC %s :%s",
					      use_id(source_p),
					      chptr->chname, "");
				sendto_server(client_p, chptr, CAP_TB|CAP_TS6, CAP_EOPMOD,
					      ":%s TB %s %ld %s :%s",
					      use_id(source_server_p),
					      chptr->chname, (long)newtopicts,
					      newtopicwho, newtopic);
			}
			if (!member)
				sendto_server(client_p, chptr, CAP_TS6, CAP_EOPMOD,
					      ":%s PART %s :Topic set for %s",
					      use_id(source_p),
					      chptr->chname, newtopicwho);
		}
		else if (textchange)
		{
			/* Should not send :server ETB if not all servers
			 * support EOPMOD.
			 */
			sendto_server(client_p, chptr, CAP_TS6, CAP_EOPMOD,
				      ":%s NOTICE %s :*** Notice -- Dropping topic change for %s",
				      me.id, chptr->chname, chptr->chname);
		}
	}

	return 0;
}
