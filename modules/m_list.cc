/*
 * charybdis: An advanced ircd.
 * m_list_safelist.c: Version of /list that uses the safelist code.
 *
 * Copyright (c) 2006 William Pitcock <nenolod@nenolod.net>
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 * 1. Redistributions of source code must retain the above copyright notice,
 *    this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * 3. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
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

static const char list_desc[] = "Provides the LIST command to clients to view non-hidden channels";

static rb_dlink_list safelisting_clients = { NULL, NULL, 0 };

static struct ev_entry *iterate_clients_ev = NULL;

static int _modinit(void);
static void _moddeinit(void);

static void m_list(struct MsgBuf *, client::client &, client::client &, int, const char **);
static void mo_list(struct MsgBuf *, client::client &, client::client &, int, const char **);

static void list_one_channel(client::client &source, chan::chan *chptr, int visible);

static void safelist_one_channel(client::client &source, chan::chan *chptr, struct ListClient *params);
static void safelist_check_cliexit(hook_data_client_exit * hdata);
static void safelist_client_instantiate(client::client &, struct ListClient *);
static void safelist_client_release(client::client &);
static void safelist_iterate_client(client::client &source);
static void safelist_iterate_clients(void *unused);
static void safelist_channel_named(client::client &source, const char *name, int operspy);

struct Message list_msgtab = {
	"LIST", 0, 0, 0, 0,
	{mg_unreg, {m_list, 0}, mg_ignore, mg_ignore, mg_ignore, {mo_list, 0}}
};

mapi_clist_av1 list_clist[] = { &list_msgtab, NULL };

mapi_hfn_list_av1 list_hfnlist[] = {
	{"client_exit", (hookfn) safelist_check_cliexit},
	{NULL, NULL}
};

DECLARE_MODULE_AV2(list, _modinit, _moddeinit, list_clist, NULL, list_hfnlist, NULL, NULL, list_desc);

static int _modinit(void)
{
	iterate_clients_ev = rb_event_add("safelist_iterate_clients", safelist_iterate_clients, NULL, 3);

	/* ELIST=[tokens]:
	 *
	 * M = mask search
	 * N = !mask search
	 * U = user count search (< >)
	 * C = creation time search (C> C<)
	 * T = topic search (T> T<)
	 */
	supported::add("SAFELIST");
	supported::add("ELIST", "CTU");

	return 0;
}

static void _moddeinit(void)
{
	rb_event_delete(iterate_clients_ev);

	supported::del("SAFELIST");
	supported::del("ELIST");
}

static void safelist_check_cliexit(hook_data_client_exit * hdata)
{
	/* Cancel the safelist request if we are disconnecting
	 * from the server. That way it doesn't core. :P --nenolod
	 */
	if (my(*hdata->target) && hdata->target->localClient->safelist_data != NULL)
		safelist_client_release(*hdata->target);
}

/* m_list()
 *      parv[1] = channel
 *
 * XXX - With SAFELIST, do we really need to continue pacing?
 *       In theory, the server cannot be lagged by this. --nenolod
 */
static void
m_list(struct MsgBuf *msgbuf_p, client::client &client, client::client &source, int parc, const char *parv[])
{
	static time_t last_used = 0L;

	if (source.localClient->safelist_data != NULL)
	{
		sendto_one_notice(&source, ":/LIST aborted");
		safelist_client_release(source);
		return;
	}

	if (parc < 2 || chan::has_prefix(parv[1]))
	{
		/* pace this due to the sheer traffic involved */
		if (((last_used + ConfigFileEntry.pace_wait) > rb_current_time()))
		{
			sendto_one(&source, form_str(RPL_LOAD2HI), me.name, source.name, "LIST");
			sendto_one(&source, form_str(RPL_LISTEND), me.name, source.name);
			return;
		}
		else
			last_used = rb_current_time();
	}

	mo_list(msgbuf_p, client, source, parc, parv);
}

/* mo_list()
 *      parv[1] = channel
 */
static void
mo_list(struct MsgBuf *msgbuf_p, client::client &client, client::client &source, int parc, const char *parv[])
{
	struct ListClient *params;
	char *p;
	char *args = NULL;
	int i;
	int operspy = 0;

	if (source.localClient->safelist_data != NULL)
	{
		sendto_one_notice(&source, ":/LIST aborted");
		safelist_client_release(source);
		return;
	}

	if (parc > 1)
	{
		args = LOCAL_COPY(parv[1]);
	}

	if (args && *args == '!' && IsOperSpy(&source))
	{
		args++;
		//report_operspy(&source, "LIST", args);
		operspy = 1;
	}

	/* Single channel. */
	if (args && chan::has_prefix(args))
	{
		safelist_channel_named(source, args, operspy);
		return;
	}

	/* Multiple channels, possibly with parameters. */
	params = (ListClient *)rb_malloc(sizeof(struct ListClient));

	params->users_min = ConfigChannel.displayed_usercount;
	params->users_max = INT_MAX;
	params->operspy = operspy;
	params->created_min = params->topic_min =
		params->created_max = params->topic_max = 0;

	if (args && !EmptyString(args))
	{
		/* Cancel out default minimum. */
		params->users_min = 0;

		for (i = 0; i < 7; i++)
		{
			if ((p = strchr(args, ',')) != NULL)
				*p++ = '\0';

			if (*args == '<')
			{
				args++;
				if (rfc1459::is_digit(*args))
				{
					params->users_max = atoi(args);
					if (params->users_max == 0)
						params->users_max = INT_MAX;
					else
						params->users_max--;
				}
			}
			else if (*args == '>')
			{
				args++;
				if (rfc1459::is_digit(*args))
					params->users_min = atoi(args) + 1;
				else
					params->users_min = 0;
			}
			else if (*args == 'C' || *args == 'c')
			{
				args++;
				if (*args == '>')
				{
					/* Creation time earlier than last x minutes. */
					args++;
					if (rfc1459::is_digit(*args))
					{
						params->created_max = rb_current_time() - (60 * atoi(args));
					}
				}
				else if (*args == '<')
				{
					/* Creation time within last x minutes. */
					args++;
					if (rfc1459::is_digit(*args))
					{
						params->created_min = rb_current_time() - (60 * atoi(args));
					}
				}
			}
			else if (*args == 'T' || *args == 't')
			{
				args++;
				if (*args == '>')
				{
					/* Topic change time earlier than last x minutes. */
					args++;
					if (rfc1459::is_digit(*args))
					{
						params->topic_max = rb_current_time() - (60 * atoi(args));
					}
				}
				else if (*args == '<')
				{
					/* Topic change time within last x minutes. */
					args++;
					if (rfc1459::is_digit(*args))
					{
						params->topic_min = rb_current_time() - (60 * atoi(args));
					}
				}
			}

			if (EmptyString(p))
				break;
			else
				args = p;
		}
	}

	safelist_client_instantiate(source, params);
}

/*
 * list_one_channel()
 *
 * inputs       - client pointer, channel pointer, whether normally visible
 * outputs      - none
 * side effects - a channel is listed
 */
static void list_one_channel(client::client &source, chan::chan *chptr,
		int visible)
{
	char topic[TOPICLEN + 1];

	if (!chptr->topic.text.empty())
		rb_strlcpy(topic, chptr->topic.text.c_str(), sizeof topic);
	else
		topic[0] = '\0';
	strip_colour(topic);
	sendto_one(&source, form_str(RPL_LIST), me.name, source.name,
		   visible ? "" : "!",
		   chptr->name.c_str(), size(chptr->members),
		   topic);
}

/*
 * safelist_sendq_exceeded()
 *
 * inputs       - pointer to client that needs checking
 * outputs      - true if a client has exceeded the reserved
 *                sendq limit, false if not
 * side effects - none
 *
 * When safelisting, we only use half of the SendQ at any
 * given time.
 */
static bool safelist_sendq_exceeded(client::client &client)
{
	return rb_linebuf_len(&client.localClient->buf_sendq) > (get_sendq(&client) / 2);
}

/*
 * safelist_client_instantiate()
 *
 * inputs       - pointer to Client to be listed,
 *                pointer to ListClient for params
 * outputs      - none
 * side effects - the safelist process begins for a
 *                client.
 *
 * Please do not ever call this on a non-local client.
 * If you do, you will get SIGSEGV.
 */
static void safelist_client_instantiate(client::client &client, struct ListClient *params)
{
	s_assert(my(client));
	s_assert(params != NULL);

	client.localClient->safelist_data = params;

	sendto_one(&client, form_str(RPL_LISTSTART), me.name, client.name);

	/* pop the client onto the queue for processing */
	rb_dlinkAddAlloc(&client, &safelisting_clients);

	/* give the user some initial data to work with */
	safelist_iterate_client(client);
}

/*
 * safelist_client_release()
 *
 * inputs       - pointer to Client being listed on
 * outputs      - none
 * side effects - the client is no longer being
 *                listed
 */
static void safelist_client_release(client::client &client)
{
	if(!my(client))
		return;

	s_assert(my(client));

	rb_dlinkFindDestroy(&client, &safelisting_clients);

	rb_free(client.localClient->safelist_data->chname);
	rb_free(client.localClient->safelist_data);

	client.localClient->safelist_data = NULL;

	sendto_one(&client, form_str(RPL_LISTEND), me.name, client.name);
}

/*
 * safelist_channel_named()
 *
 * inputs       - client pointer, channel name, operspy
 * outputs      - none
 * side effects - a named channel is listed
 */
static void safelist_channel_named(client::client &source, const char *name, int operspy)
{
	chan::chan *chptr;
	char *p;
	int visible;

	sendto_one(&source, form_str(RPL_LISTSTART), me.name, source.name);

	if ((p = (char *)strchr(name, ',')))
		*p = '\0';

	if (*name == '\0')
	{
		sendto_one_numeric(&source, ERR_NOSUCHNICK, form_str(ERR_NOSUCHNICK), name);
		sendto_one(&source, form_str(RPL_LISTEND), me.name, source.name);
		return;
	}

	chptr = chan::get(name, std::nothrow);

	if (chptr == NULL)
	{
		sendto_one_numeric(&source, ERR_NOSUCHNICK, form_str(ERR_NOSUCHNICK), name);
		sendto_one(&source, form_str(RPL_LISTEND), me.name, source.name);
		return;
	}

	visible = !is_secret(chptr) || is_member(chptr, &source);
	if (visible || operspy)
		list_one_channel(source, chptr, visible);

	sendto_one(&source, form_str(RPL_LISTEND), me.name, source.name);
	return;
}

/*
 * safelist_one_channel()
 *
 * inputs       - client pointer and channel pointer
 * outputs      - none
 * side effects - a channel is listed if it meets the
 *                requirements
 */
static void safelist_one_channel(client::client &source, chan::chan *chptr, struct ListClient *params)
{
	int visible;

	visible = !is_secret(chptr) || is_member(chptr, &source);
	if (!visible && !params->operspy)
		return;

	if (size(chptr->members) < params->users_min || size(chptr->members) > params->users_max)
		return;

	if (params->topic_min && chptr->topic.time < params->topic_min)
		return;

	/* If a topic TS is provided, don't show channels without a topic set. */
	if (params->topic_max && (chptr->topic.time > params->topic_max || chptr->topic.time == 0))
		return;

	if (params->created_min && chptr->channelts < params->created_min)
		return;

	if (params->created_max && chptr->channelts > params->created_max)
		return;

	list_one_channel(source, chptr, visible);
}

/*
 * safelist_iterate_client()
 *
 * inputs       - client pointer
 * outputs      - none
 * side effects - the client's sendq is filled up again
 */
static void safelist_iterate_client(client::client &source)
{
	std::string chname(source.localClient->safelist_data->chname);
	auto it(chan::chans.lower_bound(&chname));
	for (; it != end(chan::chans); ++it)
	{
		auto &chan(*it->second);
		if (safelist_sendq_exceeded(*source.from))
		{
			rb_free(source.localClient->safelist_data->chname);
			chname = rb_strdup(name(chan).c_str());
			return;
		}

		safelist_one_channel(source, &chan, source.localClient->safelist_data);
	}

	safelist_client_release(source);
}

static void safelist_iterate_clients(void *unused)
{
	rb_dlink_node *n, *n2;

	RB_DLINK_FOREACH_SAFE(n, n2, safelisting_clients.head)
		safelist_iterate_client(*(client::client *)n->data);
}
