/*
 * charybdis: an advanced ircd.
 * cap_batch.c: implement the batch IRCv3.2 capability
 *
 * Copyright (c) 2016 Elizabeth Myers <elizabeth@interlinked.me>
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice is present in all copies.
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

#include "stdinc.h"
#include "modules.h"
#include "hook.h"
#include "client.h"
#include "ircd.h"
#include "send.h"
#include "s_conf.h"
#include "s_user.h"
#include "s_serv.h"
#include "numeric.h"
#include "chmode.h"
#include "batch.h"
#include "inline/stringops.h"

static const char cap_batch_desc[] =
	"Provides the batch client capability";

static void cap_batch_process(hook_data *);

mapi_hfn_list_av1 cap_batch_hfnlist[] = {
	{ "outbound_msgbuf", (hookfn) cap_batch_process },
	{ NULL, NULL }
};

static void
cap_batch_process(hook_data *data)
{
	struct MsgBuf *msgbuf = data->arg1;
	struct Client *client_p = data->client;
	struct Batch *batch_p;

	if(rb_strcasecmp(msgbuf->cmd, "quit") == 0)
	{
		if(!IsClient(client_p) || MyConnect(client_p))
			/* Remote users only please */
			return;

		/* Now find our batch... */
		if((batch_p = find_batch(BATCH_NETSPLIT, client_p->from)) == NULL)
			return;

		/* Boom */
		msgbuf_append_tag(msgbuf, "batch", batch_p->id, CLICAP_BATCH);
	}
}

DECLARE_MODULE_AV2(cap_batch, NULL, NULL, NULL, NULL, cap_batch_hfnlist, NULL, NULL, cap_batch_desc);
