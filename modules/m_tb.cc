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
 */

using namespace ircd;

static const char tb_desc[] =
	"Provides TS6 TB and ETB commands for topic bursting between servers";

static void ms_tb(struct MsgBuf *msgbuf_p, client::client &client, client::client &source, int parc, const char *parv[]);
static void ms_etb(struct MsgBuf *msgbuf_p, client::client &client, client::client &source, int parc, const char *parv[]);

struct Message tb_msgtab = {
	"TB", 0, 0, 0, 0,
	{mg_unreg, mg_ignore, mg_ignore, {ms_tb, 4}, mg_ignore, mg_ignore}
};

struct Message etb_msgtab = {
	"ETB", 0, 0, 0, 0,
	{mg_unreg, mg_ignore, {ms_etb, 5}, {ms_etb, 5}, mg_ignore, mg_ignore}
};

mapi_clist_av1 tb_clist[] =  { &tb_msgtab, &etb_msgtab, NULL };
DECLARE_MODULE_AV2(tb, NULL, NULL, tb_clist, NULL, NULL, NULL, NULL, tb_desc);

/* m_tb()
 *
 * parv[1] - channel
 * parv[2] - topic ts
 * parv[3] - optional topicwho/topic
 * parv[4] - topic
 */
static void
ms_tb(struct MsgBuf *msgbuf_p, client::client &client, client::client &source, int parc, const char *parv[])
{
	chan::chan *chptr;
	const char *newtopic;
	const char *newtopicwho;
	time_t newtopicts;
	client::client *fake_source;

	chptr = chan::get(parv[1], std::nothrow);

	if(chptr == NULL)
		return;

	newtopicts = atol(parv[2]);

	/* Hide connecting server on netburst -- jilles */
	if (ConfigServerHide.flatten_links && !has_sent_eob(source))
		fake_source = &me;
	else
		fake_source = &source;

	if(parc == 5)
	{
		newtopic = parv[4];
		newtopicwho = parv[3];
	}
	else
	{
		newtopic = parv[3];
		newtopicwho = fake_source->name;
	}

	if (EmptyString(newtopic))
		return;

	if(!chptr->topic || chptr->topic.time > newtopicts)
	{
		/* its possible the topicts is a few seconds out on some
		 * servers, due to lag when propagating it, so if theyre the
		 * same topic just drop the message --fl
		 */
		if(chptr->topic && strcmp(chptr->topic.text.c_str(), newtopic) == 0)
			return;

		set_channel_topic(chptr, newtopic, newtopicwho, newtopicts);
		sendto_channel_local(chan::ALL_MEMBERS, chptr, ":%s TOPIC %s :%s",
				     fake_source->name, chptr->name.c_str(), newtopic);
		sendto_server(&client, chptr, CAP_TB|CAP_TS6, NOCAPS,
			      ":%s TB %s %ld %s%s:%s",
			      use_id(&source), chptr->name.c_str(), (long) chptr->topic.time,
			      ConfigChannel.burst_topicwho ? chptr->topic.info.c_str() : "",
			      ConfigChannel.burst_topicwho ? " " : "", chptr->topic.text.c_str());
	}
}

/* ms_etb()
 *
 * parv[1] - channel ts
 * parv[2] - channel
 * parv[3] - topic ts
 * parv[4] - topicwho
 * parv[5] - topic
 */
static void
ms_etb(struct MsgBuf *msgbuf_p, client::client &client, client::client &source, int parc, const char *parv[])
{
	chan::chan *chptr;
	const char *newtopic;
	const char *newtopicwho;
	time_t channelts, newtopicts;
	client::client *fake_source, *source_server_p;
	int textchange, can_use_tb, member;

	channelts = atol(parv[1]);
	chptr = chan::get(parv[2], std::nothrow);

	if(chptr == NULL)
		return;

	newtopicts = atol(parv[3]);

	/* Hide connecting server on netburst -- jilles */
	if (is_server(source) && ConfigServerHide.flatten_links &&
			!has_sent_eob(source))
		fake_source = &me;
	else
		fake_source = &source;

	newtopicwho = parv[4];
	newtopic = parv[parc - 1];

	if(!chptr->topic || chptr->channelts > channelts ||
			(chptr->channelts == channelts && chptr->topic.time < newtopicts))
	{
		textchange = chptr->topic.text.empty() || strcmp(chptr->topic.text.c_str(), newtopic);
		can_use_tb = textchange && !EmptyString(newtopic) &&
			(chptr->topic.text.empty() || chptr->topic.time > newtopicts);

		set_channel_topic(chptr, newtopic, newtopicwho, newtopicts);
		newtopic = !chptr->topic.text.empty()? chptr->topic.text.c_str() : "";
		if (!chptr->topic.info.empty())
			newtopicwho = chptr->topic.info.c_str();  //TODO: XXX: ??

		/* Do not send a textually identical topic to clients,
		 * but do propagate the new topicts/topicwho to servers.
		 */
		if(textchange)
		{
			if (is_person(*fake_source))
				sendto_channel_local(chan::ALL_MEMBERS, chptr,
						":%s!%s@%s TOPIC %s :%s",
						fake_source->name,
						fake_source->username,
						fake_source->host,
						chptr->name.c_str(),
						newtopic);
			else
				sendto_channel_local(chan::ALL_MEMBERS, chptr,
						":%s TOPIC %s :%s",
						fake_source->name,
						chptr->name.c_str(), newtopic);
		}
		/* Propagate channelts as given, because an older channelts
		 * forces any change.
		 */
		sendto_server(&client, chptr, CAP_EOPMOD|CAP_TS6, NOCAPS,
			      ":%s ETB %ld %s %ld %s :%s",
			      use_id(&source), (long)channelts, chptr->name.c_str(),
			      (long)newtopicts, newtopicwho, newtopic);
		source_server_p = is_server(source) ? &source : source.servptr;
		if (can_use_tb)
			sendto_server(&client, chptr, CAP_TB|CAP_TS6, CAP_EOPMOD,
				      ":%s TB %s %ld %s :%s",
				      use_id(source_server_p),
				      chptr->name.c_str(), (long)newtopicts,
				      newtopicwho, newtopic);
		else if (is_person(source) && textchange)
		{
			member = is_member(chptr, &source);
			if (!member)
				sendto_server(&client, chptr, CAP_TS6, CAP_EOPMOD,
					      ":%s SJOIN %ld %s + :@%s",
					      use_id(source_server_p),
					      (long)chptr->channelts,
					      chptr->name.c_str(), use_id(&source));
			if (EmptyString(newtopic) ||
					newtopicts >= rb_current_time() - 60)
				sendto_server(&client, chptr, CAP_TS6, CAP_EOPMOD,
					      ":%s TOPIC %s :%s",
					      use_id(&source),
					      chptr->name.c_str(), newtopic);
			else
			{
				sendto_server(&client, chptr, CAP_TS6, CAP_EOPMOD,
					      ":%s TOPIC %s :%s",
					      use_id(&source),
					      chptr->name.c_str(), "");
				sendto_server(&client, chptr, CAP_TB|CAP_TS6, CAP_EOPMOD,
					      ":%s TB %s %ld %s :%s",
					      use_id(source_server_p),
					      chptr->name.c_str(), (long)newtopicts,
					      newtopicwho, newtopic);
			}
			if (!member)
				sendto_server(&client, chptr, CAP_TS6, CAP_EOPMOD,
					      ":%s PART %s :Topic set for %s",
					      use_id(&source),
					      chptr->name.c_str(), newtopicwho);
		}
		else if (textchange)
		{
			/* Should not send :server ETB if not all servers
			 * support EOPMOD.
			 */
			sendto_server(&client, chptr, CAP_TS6, CAP_EOPMOD,
				      ":%s NOTICE %s :*** Notice -- Dropping topic change for %s",
				      me.id, chptr->name.c_str(), chptr->name.c_str());
		}
	}
}
