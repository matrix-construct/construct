/************************************************************************
 * charybdis: an advanced ircd. extensions/chm_spamfilter.c
 * Copyright (C) 2016 Jason Volk
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

#include "spamfilter.h"

using namespace ircd;

int chm_spamfilter;
int h_spamfilter_query;
int h_spamfilter_reject;

char reject_reason[BUFSIZE - CHANNELLEN - 32];


static
void hook_privmsg_channel(hook_data_privmsg_channel *const hook)
{
	// Check for mootness by other hooks first
	if(hook->approved != 0 || EmptyString(hook->text))
		return;

	// Assess channel related
	if(~hook->chptr->mode.mode & chm_spamfilter)
		return;

	// Assess client related
	if(is_exempt_spambot(*hook->source_p))
		return;

	// Assess type related
	if(hook->msgtype != MESSAGE_TYPE_NOTICE && hook->msgtype != MESSAGE_TYPE_PRIVMSG)
		return;

	// Invoke the active spamfilters
	call_hook(h_spamfilter_query, hook);
	if(hook->approved == 0)
		return;

	call_hook(h_spamfilter_reject, hook);
	sendto_realops_snomask(SNO_REJ|SNO_BOTS, L_NETWIDE,
	                       "spamfilter: REJECT %s[%s@%s] on %s to %s (%s)",
	                       hook->source_p->name,
	                       hook->source_p->username,
	                       hook->source_p->orighost,
	                       hook->source_p->servptr->name,
	                       hook->chptr->name.c_str(),
	                       hook->reason?: "filter gave no reason");

	hook->reason = reject_reason;
}


static
void substitute_reject_reason(void)
{
	rb_dlink_list subs = {0};
	substitution_append_var(&subs, "network-name", ServerInfo.network_name?: "${network-name}");
	substitution_append_var(&subs, "admin-email", AdminInfo.email?: "${admin-email}");
	const char *const substituted = substitution_parse(reject_reason, &subs);
	rb_strlcpy(reject_reason, substituted, sizeof(reject_reason));
	substitution_free(&subs);
}


static
void set_reject_reason(void *const str)
{
	rb_strlcpy(reject_reason, (const char *)str, sizeof(reject_reason));
	substitute_reject_reason();
}


struct ConfEntry conf_spamfilter[] =
{
	{ "reject_reason",     CF_QSTRING,   set_reject_reason, 0,  NULL },
	{ "\0",                0,            NULL,              0,  NULL }
};


static
int modinit(void)
{
	using namespace chan::mode;

	chm_spamfilter = add(MODE_SPAMFILTER, category::D, functor::simple);
	if(!chm_spamfilter)
		return -1;

	add_top_conf("spamfilter", NULL, NULL, conf_spamfilter);
	return 0;
}


static
void modfini(void)
{
	remove_top_conf("spamfilter");
	chan::mode::orphan(MODE_SPAMFILTER);
}


mapi_hlist_av1 hlist[] =
{
	{ "spamfilter_query", &h_spamfilter_query,        },
	{ "spamfilter_reject", &h_spamfilter_reject,      },
	{ NULL, NULL                                      }
};

mapi_hfn_list_av1 hfnlist[] =
{
	{ "privmsg_channel", (hookfn)hook_privmsg_channel      },
	{ NULL, NULL                                           }
};

static const char chm_spamfilter_desc[] =
	"Adds channel mode +Y which enables various spam mitigations";

DECLARE_MODULE_AV2
(
	chm_spamfilter,
	modinit,
	modfini,
	NULL,
	hlist,
	hfnlist,
	NULL,
	NULL,
	chm_spamfilter_desc
);
