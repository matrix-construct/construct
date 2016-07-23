/*
 * charybdis: an advanced ircd.
 * cap_account_tag.c: implement the account-tag IRCv3.2 capability
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

#include <ircd/stdinc.h>
#include <ircd/modules.h>
#include <ircd/hook.h>
#include <ircd/client.h>
#include <ircd/ircd.h>
#include <ircd/send.h>
#include <ircd/s_conf.h>
#include <ircd/s_user.h>
#include <ircd/s_serv.h>
#include <ircd/numeric.h>
#include <ircd/chmode.h>
#include <ircd/inline/stringops.h>

static const char cap_account_tag_desc[] =
	"Provides the account-tag client capability";

static void cap_account_tag_process(hook_data *);
unsigned int CLICAP_ACCOUNT_TAG = 0;

mapi_hfn_list_av1 cap_account_tag_hfnlist[] = {
	{ "outbound_msgbuf", (hookfn) cap_account_tag_process },
	{ NULL, NULL }
};
mapi_cap_list_av2 cap_account_tag_cap_list[] = {
	{ MAPI_CAP_CLIENT, "account-tag", NULL, &CLICAP_ACCOUNT_TAG },
	{ 0, NULL, NULL, NULL },
};
static void
cap_account_tag_process(hook_data *data)
{
	struct MsgBuf *msgbuf = (MsgBuf *)data->arg1;

	if (data->client != NULL && IsPerson(data->client) && *data->client->user->suser)
		msgbuf_append_tag(msgbuf, "account", data->client->user->suser, CLICAP_ACCOUNT_TAG);
}

DECLARE_MODULE_AV2(cap_account_tag, NULL, NULL, NULL, NULL, cap_account_tag_hfnlist, cap_account_tag_cap_list, NULL, cap_account_tag_desc);
