/* modules/m_sasl.c
 *   Copyright (C) 2006 Michael Tharp <gxti@partiallystapled.com>
 *   Copyright (C) 2006 charybdis development team
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

static const char sasl_desc[] = "Provides SASL authentication support";

static void m_authenticate(struct MsgBuf *, client::client &, client::client &, int, const char **);
static void me_sasl(struct MsgBuf *, client::client &, client::client &, int, const char **);
static void me_mechlist(struct MsgBuf *, client::client &, client::client &, int, const char **);

static void abort_sasl(client::client *);
static void abort_sasl_exit(hook_data_client_exit *);

static void advertise_sasl(client::client *);
static void advertise_sasl_exit(hook_data_client_exit *);

static unsigned int CLICAP_SASL = 0;
static char mechlist_buf[BUFSIZE];

struct Message authenticate_msgtab = {
	"AUTHENTICATE", 0, 0, 0, 0,
	{{m_authenticate, 2}, {m_authenticate, 2}, mg_ignore, mg_ignore, mg_ignore, {m_authenticate, 2}}
};
struct Message sasl_msgtab = {
	"SASL", 0, 0, 0, 0,
	{mg_ignore, mg_ignore, mg_ignore, mg_ignore, {me_sasl, 5}, mg_ignore}
};
struct Message mechlist_msgtab = {
	"MECHLIST", 0, 0, 0, 0,
	{mg_ignore, mg_ignore, mg_ignore, mg_ignore, {me_mechlist, 2}, mg_ignore}
};

mapi_clist_av1 sasl_clist[] = {
	&authenticate_msgtab, &sasl_msgtab, &mechlist_msgtab, NULL
};
mapi_hfn_list_av1 sasl_hfnlist[] = {
	{ "new_local_user",	(hookfn) abort_sasl },
	{ "client_exit",	(hookfn) abort_sasl_exit },
	{ "new_remote_user",	(hookfn) advertise_sasl },
	{ "client_exit",	(hookfn) advertise_sasl_exit },
	{ NULL, NULL }
};

static bool
sasl_visible(client::client *client)
{
	client::client *agent_p = NULL;

	if (ConfigFileEntry.sasl_service)
		agent_p = find_named_client(ConfigFileEntry.sasl_service);

	return agent_p != NULL && is(*agent_p, umode::SERVICE);
}

static const char *
sasl_data(client::client *client)
{
	return *mechlist_buf != 0 ? mechlist_buf : NULL;
}

static struct ClientCapability capdata_sasl = {
	.visible = sasl_visible,
	.data = sasl_data,
	.flags = CLICAP_FLAGS_STICKY,
};

static int
_modinit(void)
{
	memset(mechlist_buf, 0, sizeof mechlist_buf);
	CLICAP_SASL = cli_capindex.put("sasl", &capdata_sasl);
	return 0;
}

static void
_moddeinit(void)
{
	cli_capindex.orphan("sasl");
}

DECLARE_MODULE_AV2(sasl, _modinit, _moddeinit, sasl_clist, NULL, sasl_hfnlist, NULL, NULL, sasl_desc);

static void
m_authenticate(struct MsgBuf *msgbuf_p, client::client &client, client::client &source,
	int parc, const char *parv[])
{
	client::client *agent_p = NULL;
	client::client *saslserv_p = NULL;

	/* They really should use CAP for their own sake. */
	if(!IsCapable(&source, CLICAP_SASL))
		return;

	if(source.localClient->sasl_next_retry > rb_current_time())
	{
		sendto_one(&source, form_str(RPL_LOAD2HI), me.name, EmptyString(source.name) ? "*" : source.name, msgbuf_p->cmd);
		return;
	}

	if(strlen(client.id) == 3)
	{
		exit_client(&client, &client, &client, "Mixing client and server protocol");
		return;
	}

	saslserv_p = find_named_client(ConfigFileEntry.sasl_service);
	if(saslserv_p == NULL || !is(*saslserv_p, umode::SERVICE))
	{
		sendto_one(&source, form_str(ERR_SASLABORTED), me.name, EmptyString(source.name) ? "*" : source.name);
		return;
	}

	if(source.localClient->sasl_complete)
	{
		*source.localClient->sasl_agent = '\0';
		source.localClient->sasl_complete = 0;
	}

	if(strlen(parv[1]) > 400)
	{
		sendto_one(&source, form_str(ERR_SASLTOOLONG), me.name, EmptyString(source.name) ? "*" : source.name);
		return;
	}

	if(!*source.id)
	{
		/* Allocate a UID. */
		rb_strlcpy(source.id, client::generate_uid(), sizeof(source.id));
		add_to_id_hash(source.id, &source);
	}

	if(*source.localClient->sasl_agent)
		agent_p = find_id(source.localClient->sasl_agent);

	if(agent_p == NULL)
	{
		sendto_one(saslserv_p, ":%s ENCAP %s SASL %s %s H %s %s",
					me.id, saslserv_p->servptr->name, source.id, saslserv_p->id,
					source.host, source.sockhost);

		if (source.certfp != NULL)
			sendto_one(saslserv_p, ":%s ENCAP %s SASL %s %s S %s %s",
						me.id, saslserv_p->servptr->name, source.id, saslserv_p->id,
						parv[1], source.certfp);
		else
			sendto_one(saslserv_p, ":%s ENCAP %s SASL %s %s S %s",
						me.id, saslserv_p->servptr->name, source.id, saslserv_p->id,
						parv[1]);

		rb_strlcpy(source.localClient->sasl_agent, saslserv_p->id, client::IDLEN);
	}
	else
		sendto_one(agent_p, ":%s ENCAP %s SASL %s %s C %s",
				me.id, agent_p->servptr->name, source.id, agent_p->id,
				parv[1]);
	source.localClient->sasl_out++;
}

static void
me_sasl(struct MsgBuf *msgbuf_p, client::client &client, client::client &source,
	int parc, const char *parv[])
{
	client::client *target_p, *agent_p;

	/* Let propagate if not addressed to us, or if broadcast.
	 * Only SASL agents can answer global requests.
	 */
	if(strncmp(parv[2], me.id, 3))
		return;

	if((target_p = find_id(parv[2])) == NULL)
		return;

	if((agent_p = find_id(parv[1])) == NULL)
		return;

	if(&source != agent_p->servptr) /* WTF?! */
		return;

	/* We only accept messages from SASL agents; these must have umode +S
	 * (so the server must be listed in a service{} block).
	 */
	if(!is(*agent_p, umode::SERVICE))
		return;

	/* Reject if someone has already answered. */
	if(*target_p->localClient->sasl_agent && strncmp(parv[1], target_p->localClient->sasl_agent, client::IDLEN))
		return;
	else if(!*target_p->localClient->sasl_agent)
		rb_strlcpy(target_p->localClient->sasl_agent, parv[1], client::IDLEN);

	if(*parv[3] == 'C')
	{
		sendto_one(target_p, "AUTHENTICATE %s", parv[4]);
		target_p->localClient->sasl_messages++;
	}
	else if(*parv[3] == 'D')
	{
		if(*parv[4] == 'F')
		{
			sendto_one(target_p, form_str(ERR_SASLFAIL), me.name, EmptyString(target_p->name) ? "*" : target_p->name);
			/* Failures with zero messages are just "unknown mechanism" errors; don't count those. */
			if(target_p->localClient->sasl_messages > 0)
			{
				if(*target_p->name)
				{
					target_p->localClient->sasl_failures++;
					target_p->localClient->sasl_next_retry = rb_current_time() + (1 << MIN(target_p->localClient->sasl_failures + 5, 13));
				}
				else if(throttle_add((struct sockaddr*)&target_p->localClient->ip))
				{
					exit_client(target_p, target_p, &me, "Too many failed authentication attempts");
					return;
				}
			}
		}
		else if(*parv[4] == 'S')
		{
			sendto_one(target_p, form_str(RPL_SASLSUCCESS), me.name, EmptyString(target_p->name) ? "*" : target_p->name);
			target_p->localClient->sasl_failures = 0;
			target_p->localClient->sasl_complete = 1;
			ServerStats.is_ssuc++;
		}
		*target_p->localClient->sasl_agent = '\0'; /* Blank the stored agent so someone else can answer */
		target_p->localClient->sasl_messages = 0;
	}
	else if(*parv[3] == 'M')
		sendto_one(target_p, form_str(RPL_SASLMECHS), me.name, EmptyString(target_p->name) ? "*" : target_p->name, parv[4]);
}

static void
me_mechlist(struct MsgBuf *msgbuf_p, client::client &client, client::client &source,
	    int parc, const char *parv[])
{
	rb_strlcpy(mechlist_buf, parv[1], sizeof mechlist_buf);
}

/* If the client never finished authenticating but is
 * registering anyway, abort the exchange.
 */
static void
abort_sasl(client::client *data)
{
	if(data->localClient->sasl_out == 0 || data->localClient->sasl_complete)
		return;

	data->localClient->sasl_out = data->localClient->sasl_complete = 0;
	ServerStats.is_sbad++;

	if(!is_closing(*data))
		sendto_one(data, form_str(ERR_SASLABORTED), me.name, EmptyString(data->name) ? "*" : data->name);

	if(*data->localClient->sasl_agent)
	{
		client::client *agent_p = find_id(data->localClient->sasl_agent);
		if(agent_p)
		{
			sendto_one(agent_p, ":%s ENCAP %s SASL %s %s D A", me.id, agent_p->servptr->name,
					data->id, agent_p->id);
			return;
		}
	}

	sendto_server(NULL, NULL, CAP_TS6|CAP_ENCAP, NOCAPS, ":%s ENCAP * SASL %s * D A", me.id,
			data->id);
}

static void
abort_sasl_exit(hook_data_client_exit *data)
{
	if (data->target->localClient)
		abort_sasl(data->target);
}

static void
advertise_sasl(client::client *client)
{
	if (!ConfigFileEntry.sasl_service)
		return;

	if (irccmp(client->name, ConfigFileEntry.sasl_service))
		return;

	sendto_local_clients_with_capability(CLICAP_CAP_NOTIFY, ":%s CAP * NEW :sasl", me.name);
}

static void
advertise_sasl_exit(hook_data_client_exit *data)
{
	if (!ConfigFileEntry.sasl_service)
		return;

	if (irccmp(data->target->name, ConfigFileEntry.sasl_service))
		return;

	sendto_local_clients_with_capability(CLICAP_CAP_NOTIFY, ":%s CAP * DEL :sasl", me.name);
}
