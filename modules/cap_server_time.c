/*
 * charybdis: an advanced ircd.
 * cap_server_time.c: implement the server-time IRCv3.2 capability
 *
 * Copyright (c) 2016 William Pitcock <nenolod@dereferenced.org>
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
#include "inline/stringops.h"

static void cap_server_time_process(hook_data *);

mapi_hfn_list_av1 cap_server_time_hfnlist[] = {
	{ "outbound_msgbuf", (hookfn) cap_server_time_process },
	{ NULL, NULL }
};

unsigned int CLICAP_SERVER_TIME = 0;

static void
cap_server_time_process(hook_data *data)
{
	static char buf[IRCD_BUFSIZE];
	time_t ts = rb_current_time();
	struct MsgBuf *msgbuf = data->arg1;

	strftime(buf, sizeof buf, "%Y-%m-%dT%H:%M:%S.000Z", gmtime(&ts));

	msgbuf_append_tag(msgbuf, "time", buf, CLICAP_SERVER_TIME);
}

static int
_modinit(void)
{
	CLICAP_SERVER_TIME = capability_put(cli_capindex, "server-time", NULL);
	return 0;
}

static void
_moddeinit(void)
{
	capability_orphan(cli_capindex, "server-time");
}

DECLARE_MODULE_AV2(cap_server_time, _modinit, _moddeinit, NULL, NULL, cap_server_time_hfnlist, NULL, NULL, NULL);
