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
 */

using namespace ircd;

static void h_nn_server_eob(client::client *);
static void h_nn_client_exit(hook_data_client_exit *);

mapi_hfn_list_av1 nn_hfnlist[] = {
	{ "server_eob", (hookfn) h_nn_server_eob },
	{ "client_exit", (hookfn) h_nn_client_exit },
	{ NULL, NULL }
};

static const char sno_desc[] = "Show notices about netjoins and netsplits";

DECLARE_MODULE_AV2(networknotice, NULL, NULL, NULL, NULL, nn_hfnlist, NULL, NULL, sno_desc);

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
count_mark_downlinks(client::client *server_p, int *pservcount, int *pusercount)
{
	rb_dlink_node *ptr;

	SetFloodDone(server_p);
	(*pservcount)++;
	*pusercount += users(serv(*server_p)).size();
	for(auto &client : servers(serv(*server_p)))
		count_mark_downlinks(client, pservcount, pusercount);
}

static void
h_nn_server_eob(client::client *source)
{
	int s = 0, u = 0;

	if (IsFloodDone(source))
		return;

	count_mark_downlinks(source, &s, &u);
	sendto_realops_snomask(SNO_GENERAL, L_ALL, "Netjoin %s <-> %s (%dS %dC)",
			source->servptr ? source->servptr->name : "?",
			source->name, s, u);
}

static void
h_nn_client_exit(hook_data_client_exit *hdata)
{
	client::client *source;
	int s = 0, u = 0;
	char *fromnick;

	source = hdata->target;
	fromnick = IsClient(hdata->from) ? hdata->from->name : NULL;

	if (!IsServer(source))
		return;
	if (HasSentEob(source))
	{
		count_mark_downlinks(source, &s, &u);
		sendto_realops_snomask(SNO_GENERAL, L_ALL, "Netsplit %s <-> %s (%dS %dC) (%s%s%s%s)",
				source->servptr ? source->servptr->name : "?",
				source->name, s, u,
				fromnick ? "by " : "",
				fromnick ? fromnick : "",
				fromnick ? ": " : "",
				hdata->comment);
	}
	else
		sendto_realops_snomask(SNO_GENERAL, L_ALL, "Netsplit %s <-> %s (during burst) (%s%s%s%s)",
				source->servptr ? source->servptr->name : "?",
				source->name,
				fromnick ? "by " : "",
				fromnick ? fromnick : "",
				fromnick ? ": " : "",
				hdata->comment);
}
