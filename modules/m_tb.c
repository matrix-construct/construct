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
#include "tools.h"
#include "send.h"
#include "channel.h"
#include "client.h"
#include "common.h"
#include "config.h"
#include "ircd.h"
#include "irc_string.h"
#include "s_conf.h"
#include "msg.h"
#include "modules.h"
#include "hash.h"
#include "s_serv.h"

static int ms_tb(struct Client *client_p, struct Client *source_p, int parc, const char *parv[]);

struct Message tb_msgtab = {
	"TB", 0, 0, 0, MFLG_SLOW,
	{mg_unreg, mg_ignore, mg_ignore, {ms_tb, 4}, mg_ignore, mg_ignore}
};

mapi_clist_av1 tb_clist[] =  { &tb_msgtab, NULL };
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
		sendto_server(client_p, chptr, CAP_TB, CAP_TS6,
			      ":%s TB %s %ld %s%s:%s",
			      source_p->name, chptr->chname, (long) chptr->topic_time,
			      ConfigChannel.burst_topicwho ? chptr->topic_info : "",
			      ConfigChannel.burst_topicwho ? " " : "", chptr->topic);
	}

	return 0;
}
