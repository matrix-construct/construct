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
 *
 * $Id: m_sasl.c 1409 2006-05-21 14:46:17Z jilles $
 */

#include "stdinc.h"

#include "client.h"
#include "hash.h"
#include "send.h"
#include "msg.h"
#include "modules.h"
#include "numeric.h"
#include "s_serv.h"
#include "s_stats.h"
#include "string.h"

static int mr_authenticate(struct Client *, struct Client *, int, const char **);
static int me_sasl(struct Client *, struct Client *, int, const char **);

static void abort_sasl(struct Client *);
static void abort_sasl_exit(hook_data_client_exit *);

struct Message authenticate_msgtab = {
	"AUTHENTICATE", 0, 0, 0, MFLG_SLOW,
	{{mr_authenticate, 2}, mg_reg, mg_ignore, mg_ignore, mg_ignore, mg_reg}
};
struct Message sasl_msgtab = {
	"SASL", 0, 0, 0, MFLG_SLOW,
	{mg_ignore, mg_ignore, mg_ignore, mg_ignore, {me_sasl, 5}, mg_ignore}
};

mapi_clist_av1 sasl_clist[] = { 
	&authenticate_msgtab, &sasl_msgtab, NULL
};
mapi_hfn_list_av1 sasl_hfnlist[] = {
	{ "new_local_user",	(hookfn) abort_sasl },
	{ "client_exit",	(hookfn) abort_sasl_exit },
	{ NULL, NULL }
};

DECLARE_MODULE_AV1(sasl, NULL, NULL, sasl_clist, NULL, sasl_hfnlist, "$Revision: 1409 $");

static int
mr_authenticate(struct Client *client_p, struct Client *source_p,
	int parc, const char *parv[])
{
	struct Client *agent_p = NULL;

	/* They really should use CAP for their own sake. */
	if(!IsCapable(source_p, CLICAP_SASL))
		return 0;

	if (strlen(client_p->id) == 3)
	{
		exit_client(client_p, client_p, client_p, "Mixing client and server protocol");
		return 0;
	}

	if(source_p->preClient->sasl_complete)
	{
		sendto_one(source_p, form_str(ERR_SASLALREADY), me.name, EmptyString(source_p->name) ? "*" : source_p->name);
		return 0;
	}

	if(strlen(parv[1]) > 400)
	{
		sendto_one(source_p, form_str(ERR_SASLTOOLONG), me.name, EmptyString(source_p->name) ? "*" : source_p->name);
		return 0;
	}

	if(!*source_p->id)
	{
		/* Allocate a UID. */
		strcpy(source_p->id, generate_uid());
		add_to_id_hash(source_p->id, source_p);
	}

	if(*source_p->preClient->sasl_agent)
		agent_p = find_id(source_p->preClient->sasl_agent);

	if(agent_p == NULL)
		sendto_server(NULL, NULL, CAP_TS6|CAP_ENCAP, NOCAPS, ":%s ENCAP * SASL %s * S %s", me.id,
				source_p->id, parv[1]);
	else
		sendto_one(agent_p, ":%s ENCAP %s SASL %s %s C %s", me.id, agent_p->servptr->name,
				source_p->id, agent_p->id, parv[1]);
	source_p->preClient->sasl_out++;

	return 0;
}

static int
me_sasl(struct Client *client_p, struct Client *source_p,
	int parc, const char *parv[])
{
	struct Client *target_p, *agent_p;

	/* Let propagate if not addressed to us, or if broadcast.
	 * Only SASL agents can answer global requests.
	 */
	if(strncmp(parv[2], me.id, 3))
		return 0;

	if((target_p = find_id(parv[2])) == NULL)
		return 0;

	if(target_p->preClient == NULL)
		return 0;

	if((agent_p = find_id(parv[1])) == NULL)
		return 0;

	if(source_p != agent_p->servptr) /* WTF?! */
		return 0;

	/* We only accept messages from SASL agents; these must have umode +S
	 * (so the server must be listed in a service{} block).
	 */
	if(!IsService(agent_p))
		return 0;

	/* Reject if someone has already answered. */
	if(*target_p->preClient->sasl_agent && strncmp(parv[1], target_p->preClient->sasl_agent, IDLEN))
		return 0;
	else if(!*target_p->preClient->sasl_agent)
		rb_strlcpy(target_p->preClient->sasl_agent, parv[1], IDLEN);

	if(*parv[3] == 'C')
		sendto_one(target_p, "AUTHENTICATE %s", parv[4]);
	else if(*parv[3] == 'D')
	{
		if(*parv[4] == 'F')
			sendto_one(target_p, form_str(ERR_SASLFAIL), me.name, EmptyString(target_p->name) ? "*" : target_p->name);
		else if(*parv[4] == 'S') {
			sendto_one(target_p, form_str(RPL_SASLSUCCESS), me.name, EmptyString(target_p->name) ? "*" : target_p->name);
			target_p->preClient->sasl_complete = 1;
			ServerStats.is_ssuc++;
		}
		*target_p->preClient->sasl_agent = '\0'; /* Blank the stored agent so someone else can answer */
	}
	
	return 0;
}

/* If the client never finished authenticating but is
 * registering anyway, abort the exchange.
 */
static void
abort_sasl(struct Client *data)
{
	if(data->preClient->sasl_out == 0 || data->preClient->sasl_complete)
		return;

	data->preClient->sasl_out = data->preClient->sasl_complete = 0;
	ServerStats.is_sbad++;

	if(!IsClosing(data))
		sendto_one(data, form_str(ERR_SASLABORTED), me.name, EmptyString(data->name) ? "*" : data->name);

	if(*data->preClient->sasl_agent)
	{
		struct Client *agent_p = find_id(data->preClient->sasl_agent);
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
	if (data->target->preClient)
		abort_sasl(data->target);
}

