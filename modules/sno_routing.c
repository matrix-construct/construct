/*
 *  charybdis: an advanced Internet Relay Chat Daemon(ircd).
 *  sno_routing.c: Shows notices about netjoins and netsplits
 *
 *  Copyright (c) 2005-2006 Jilles Tjoelker <jilles-at-stack.nl>
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
 *  $Id: sno_routing.c 1172 2006-04-18 13:49:18Z jilles $
 */

#include "stdinc.h"
#include "modules.h"
#include "client.h"
#include "hook.h"
#include "ircd.h"
#include "send.h"

static void h_nn_server_eob(struct Client *);
static void h_nn_client_exit(hook_data_client_exit *);

mapi_hfn_list_av1 nn_hfnlist[] = {
	{ "server_eob", (hookfn) h_nn_server_eob },
	{ "client_exit", (hookfn) h_nn_client_exit },
	{ NULL, NULL }
};

DECLARE_MODULE_AV1(networknotice, NULL, NULL, NULL, NULL, nn_hfnlist, "$Revision: 1172 $");

/*
 * count_mark_downlinks
 *
 * inputs	- pointer to server to count
 *		- pointers to server and user count
 * output	- NONE
 * side effects - servers are marked
 * 		- server and user counts are added to given values
 */
static void
count_mark_downlinks(struct Client *server_p, int *pservcount, int *pusercount)
{
	rb_dlink_node *ptr;

	SetFloodDone(server_p);
	(*pservcount)++;
	*pusercount += rb_dlink_list_length(&server_p->serv->users);
	RB_DLINK_FOREACH(ptr, server_p->serv->servers.head)
	{
		count_mark_downlinks(ptr->data, pservcount, pusercount);
	}
}

static void
h_nn_server_eob(struct Client *source_p)
{
	int s = 0, u = 0;

	if (IsFloodDone(source_p))
		return;
	count_mark_downlinks(source_p, &s, &u);
	sendto_realops_snomask(SNO_GENERAL, L_ALL, "Netjoin %s <-> %s (%dS %dC)",
			source_p->servptr ? source_p->servptr->name : "?",
			source_p->name, s, u);
}

static void
h_nn_client_exit(hook_data_client_exit *hdata)
{
	struct Client *source_p;
	int s = 0, u = 0;
	char *fromnick;

	source_p = hdata->target;
	fromnick = IsClient(hdata->from) ? hdata->from->name : NULL;

	if (!IsServer(source_p))
		return;
	if (HasSentEob(source_p))
	{
		count_mark_downlinks(source_p, &s, &u);
		sendto_realops_snomask(SNO_GENERAL, L_ALL, "Netsplit %s <-> %s (%dS %dC) (%s%s%s%s)",
				source_p->servptr ? source_p->servptr->name : "?",
				source_p->name, s, u,
				fromnick ? "by " : "",
				fromnick ? fromnick : "",
				fromnick ? ": " : "",
				hdata->comment);
	}
	else
		sendto_realops_snomask(SNO_GENERAL, L_ALL, "Netsplit %s <-> %s (during burst) (%s%s%s%s)",
				source_p->servptr ? source_p->servptr->name : "?",
				source_p->name,
				fromnick ? "by " : "",
				fromnick ? fromnick : "",
				fromnick ? ": " : "",
				hdata->comment);
}
